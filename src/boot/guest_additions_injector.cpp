#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

#include "ce_image_relocator.h"
#include "ce_import_binder.h"
#include "guest_additions_binaries.h"
#include "guest_module_placer.h"
#include "guest_stub_host.h"
#include "guest_xip_image.h"
#include "imgfs_injector.h"
#include "pe_image.h"
#include "rom_record_layout.h"
#include "rom_toc_lookup.h"

#include <cerfcore/core/board_database.h>
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/host_file_bytes.h>
#include <cerfcore/core/log.h>
#include <cerfcore/core/service.h>
#include <cerfcore/cpu/emulated_memory.h>
#include "../boards/page_table_builder.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace cerfcore {

namespace {

constexpr uint32_t kImgScnMemShared = 0x10000000u;
constexpr uint32_t kImgScnMemWrite  = 0x80000000u;

uint32_t Align4(uint32_t v) { return (v + 3u) & ~3u; }

class GuestAdditionsInjector : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override {
        return emu_.Get<DeviceConfig>().guest_additions && emu_.TryGet<GuestXipImage>() != nullptr;
    }

    void OnReady() override {
        xip_ = emu_.Get<GuestXipImage>().Primary();
        if (!xip_) {
            LOG(GuestAdditions, "the ROM has no XIP region that the injection can patch\n");
            emu_.Get<Fatal>().UserError("Guest Additions can't run on this ROM");
        }

        ce_major_ = DetectCeMajor();
        layout_ = (ce_major_ <= 2 && ce_minor_ < 11) ? &kE32RomCE20
                : (ce_major_ <= 2)                   ? &kE32RomCE211
                : (ce_major_ <= 4)                   ? &kE32RomCE3
                                                     : &kE32RomCE5plus;
        const char* layout_name = (layout_ == &kE32RomCE20)  ? "CE2.0"
                                : (layout_ == &kE32RomCE211) ? "CE2.11"
                                : (layout_ == &kE32RomCE3)   ? "pre-CE5"
                                                             : "CE5+";
        LOG(GuestAdditions, "CE version=%u.%u; e32_rom layout=%s\n",
            ce_major_, ce_minor_, layout_name);

        const auto victims = emu_.Get<BoardDatabase>().GaVideoDriverNames();

        const std::string stub_path =
            emu_.Get<GuestAdditionsBinaries>().StubPath();

        auto* imgfs = emu_.TryGet<ImgfsInjector>();
        int matched = 0;
        for (const auto& victim : victims) {
            if (Replace(victim.c_str(), stub_path)) ++matched;
            if (imgfs && imgfs->ReplaceVictim(victim.c_str(), stub_path)) ++matched;
        }
        if (matched == 0) {
            LOG(GuestAdditions, "no display-driver victim matched\n");
            emu_.Get<Fatal>().UserError("Guest Additions can't run on this ROM");
        }
        LOG(GuestAdditions, "%d victim(s) replaced\n", matched);
    }

private:
    bool Replace(const char* victim_name, const std::string& source_path);

    bool FindVictim(const char* name, size_t& out_index, uint32_t& out_entry_kva);
    uint32_t DetectCeMajor();

    void WriteE32Rom(uint8_t* e32, const PeImage& pe, uint32_t target_vbase,
                     uint16_t subsys_major, uint16_t subsys_minor);
    void WriteO32Array(uint8_t* o32, const PeImage& pe,
                       const std::vector<uint32_t>& dataptr_kva,
                       const std::vector<uint32_t>& realaddr,
                       const std::vector<uint32_t>& flags);
    void WriteSectionBytes(std::vector<uint8_t>& foot, const PeImage& pe,
                           const std::vector<uint8_t>& bytes);

    const ParsedXipRegion* xip_     = nullptr;
    uint32_t            ce_major_ = 5;
    uint32_t            ce_minor_ = 0;
    const E32RomLayout* layout_   = &kE32RomCE5plus;
};

uint32_t GuestAdditionsInjector::DetectCeMajor() {
    uint16_t major = 0, minor = 0;
    if (!emu_.Get<GuestXipImage>().KernelSubsystemVersion(major, minor)) {
        LOG(GuestAdditions, "nk.exe e32_rom subsystem version not locatable\n");
        emu_.Get<Fatal>().UserError("Guest Additions can't run on this ROM");
    }
    if (major < 2 || major > 8) {
        LOG(GuestAdditions, "nk.exe e32_subsysmajor=%u outside CE range 2..8\n", major);
        emu_.Get<Fatal>().UserError("Guest Additions can't run on this ROM");
    }
    ce_minor_ = minor;
    return major;
}

bool GuestAdditionsInjector::FindVictim(const char* name, size_t& out_index,
                                         uint32_t& out_entry_kva) {
    const auto& toc = xip_->toc;
    const ParsedTOCentry* m = cerf::rom_toc::FindModule(toc, name);
    if (!m) return false;
    out_index     = size_t(m - toc.modules.data());
    out_entry_kva = toc.romhdr_va + kRomHdrSize + uint32_t(out_index) * kTocEntrySize;
    return true;
}

void GuestAdditionsInjector::WriteE32Rom(uint8_t* e32, const PeImage& pe,
                                          uint32_t target_vbase,
                                          uint16_t subsys_major,
                                          uint16_t subsys_minor) {
    using cerf::le::Put16;
    using cerf::le::Put32;
    const E32RomLayout& L = *layout_;

    Put16(e32 + L.off_objcnt,      uint16_t(pe.Sections().size()));
    Put16(e32 + L.off_imageflags,  pe.ImageFlags());
    Put32(e32 + L.off_entryrva,    pe.EntryRva());
    Put32(e32 + L.off_vbase,       target_vbase);
    Put16(e32 + L.off_subsysmajor, subsys_major);
    Put16(e32 + L.off_subsysminor, subsys_minor);
    if (L.off_stackmax >= 0) {
        Put32(e32 + L.off_stackmax, pe.StackReserve());
    }
    if (L.off_vsize >= 0) {
        Put32(e32 + L.off_vsize, pe.ImageSize());
    }
    if (L.off_sect14rva >= 0) {
        Put32(e32 + L.off_sect14rva,  0);
    }
    if (L.off_sect14size >= 0) {
        Put32(e32 + L.off_sect14size, 0);
    }
    if (L.off_timestamp >= 0) {
        Put32(e32 + L.off_timestamp, 0);
    }
    for (int i = 0; i < kE32UnitCount; ++i) {
        Put32(e32 + L.off_unit + uint32_t(i) * 8u + 0, pe.DirRva (i));
        Put32(e32 + L.off_unit + uint32_t(i) * 8u + 4, pe.DirSize(i));
    }
    Put16(e32 + L.off_subsys,      pe.Subsystem());
}

void GuestAdditionsInjector::WriteO32Array(uint8_t* o32, const PeImage& pe,
                                            const std::vector<uint32_t>& dataptr_kva,
                                            const std::vector<uint32_t>& realaddr,
                                            const std::vector<uint32_t>& flags) {
    using cerf::le::Put32;
    for (size_t i = 0; i < pe.Sections().size(); ++i) {
        const auto& s = pe.Sections()[i];
        uint8_t* const off = o32 + i * kO32RomSize;
        Put32(off + kO32OffVsize,    s.vsize);
        Put32(off + kO32OffRva,      s.rva);
        Put32(off + kO32OffPsize,    s.psize);
        Put32(off + kO32OffDataptr,  dataptr_kva[i]);
        Put32(off + kO32OffRealaddr, realaddr[i]);
        Put32(off + kO32OffFlags,    flags[i]);
        LOG(GuestAdditions, "  o32[%zu] vsize=0x%05X rva=0x%05X psize=0x%05X "
                  "dataptr=0x%08X realaddr=0x%08X flags=0x%08X\n",
            i, s.vsize, s.rva, s.psize, dataptr_kva[i], realaddr[i], flags[i]);
    }
}

void GuestAdditionsInjector::WriteSectionBytes(std::vector<uint8_t>& foot,
                                                const PeImage& pe,
                                                const std::vector<uint8_t>& bytes) {
    const uint64_t image_bytes = AlignRomPage(pe.ImageSize());
    for (const auto& s : pe.Sections()) {
        if (s.psize > 0 && size_t(s.pe_file_off) + s.psize <= bytes.size()) {
            if (uint64_t(s.rva) + s.psize > image_bytes) {
                LOG(GuestAdditions, "stub section rva 0x%X psize 0x%X ends past the image size "
                                    "0x%X\n", s.rva, s.psize, pe.ImageSize());
                emu_.Get<Fatal>().UserError("Guest Additions can't run on this ROM");
            }
            std::memcpy(foot.data() + s.rva, bytes.data() + s.pe_file_off, s.psize);
        }
    }
}

bool GuestAdditionsInjector::Replace(const char* victim_name,
                                      const std::string& source_path) {
    size_t   victim_idx = 0;
    uint32_t entry_kva  = 0;
    if (!FindVictim(victim_name, victim_idx, entry_kva)) {
        LOG(GuestAdditions, "'%s' not present in TOC - skip\n", victim_name);
        return false;
    }

    std::vector<uint8_t> pe_bytes;
    if (!TryReadHostFileBytes(source_path, pe_bytes)) {
        LOG(GuestAdditions, "cannot open the stub %s for victim %s\n", source_path.c_str(), victim_name);
        emu_.Get<Fatal>().UserError("Guest Additions can't run on this ROM");
    }
    const size_t pe_bytes_size = pe_bytes.size();

    PeImage pe(std::move(pe_bytes));
    if (!pe.Parsed()) {
        LOG(GuestAdditions, "PE parse failed for the stub %s, victim %s\n", source_path.c_str(), victim_name);
        emu_.Get<Fatal>().UserError("Guest Additions can't run on this ROM");
    }

    const auto& primary_toc = xip_->toc;
    auto& pt  = emu_.Get<PageTableBuilder>();
    auto& mem = emu_.Get<EmulatedMemory>();
    const E32RomLayout& L = *layout_;

    const uint32_t orig_e32_kva  = primary_toc.modules[victim_idx].ulE32Offset;
    const uint32_t orig_e32_pa   = pt.VaToPa(orig_e32_kva);
    const uint32_t orig_o32_kva  = primary_toc.modules[victim_idx].ulO32Offset;
    const uint32_t orig_o32_pa   = pt.VaToPa(orig_o32_kva);
    const uint32_t orig_vbase    = mem.ReadWord(orig_e32_pa + L.off_vbase);
    const uint16_t orig_imgflags = mem.ReadHalf(orig_e32_pa + L.off_imageflags);
    const uint16_t orig_objcnt   = mem.ReadHalf(orig_e32_pa + L.off_objcnt);
    const uint16_t orig_subsysmaj = mem.ReadHalf(orig_e32_pa + L.off_subsysmajor);
    const uint16_t orig_subsysmin = mem.ReadHalf(orig_e32_pa + L.off_subsysminor);
    if (orig_objcnt == 0) {
        LOG(GuestAdditions, "%s victim has zero sections\n", victim_name);
        emu_.Get<Fatal>().UserError("Guest Additions can't run on this ROM");
    }
    LOG(GuestAdditions, "orig %s e32 @KVA 0x%08X imageflags=0x%04X objcnt=%u "
              "subsysver=%u.%02u o32 @KVA 0x%08X\n",
        victim_name, orig_e32_kva, orig_imgflags, orig_objcnt,
        orig_subsysmaj, orig_subsysmin, orig_o32_kva);
    for (uint32_t i = 0; i < orig_objcnt && i < 16; ++i) {
        const uint32_t off = orig_o32_pa + i * kO32RomSize;
        LOG(GuestAdditions, "  ORIG o32[%u] vsize=0x%05X rva=0x%05X psize=0x%05X "
                  "dataptr=0x%08X realaddr=0x%08X flags=0x%08X\n",
            i,
            mem.ReadWord(off + kO32OffVsize),
            mem.ReadWord(off + kO32OffRva),
            mem.ReadWord(off + kO32OffPsize),
            mem.ReadWord(off + kO32OffDataptr),
            mem.ReadWord(off + kO32OffRealaddr),
            mem.ReadWord(off + kO32OffFlags));
    }

    const uint32_t code_realaddr0 = mem.ReadWord(orig_o32_pa + kO32OffRealaddr);
    const uint32_t code_rva0      = mem.ReadWord(orig_o32_pa + kO32OffRva);
    const uint32_t codebase       = code_realaddr0 - code_rva0;

    bool vbase_ok = false;
    if (ce_major_ >= 6) {
        const uint32_t um_first = (primary_toc.romhdr.dllfirst >> 16) << 16;
        const uint32_t um_last  = (primary_toc.romhdr.dlllast  >> 16) << 16;
        const uint32_t km_first = (primary_toc.romhdr.dllfirst & 0xFFFFu) << 16;
        const uint32_t km_last  = (primary_toc.romhdr.dlllast  & 0xFFFFu) << 16;
        vbase_ok = (orig_vbase >= um_first && orig_vbase < um_last)
                || (orig_vbase >= km_first && orig_vbase < km_last);
    } else {
        const bool in_dll_region = (orig_vbase >= primary_toc.romhdr.dllfirst)
                                && (orig_vbase <  primary_toc.romhdr.dlllast);
        const bool xip_codebase  = (orig_vbase != 0)
                                && ((orig_vbase & kRomPageMask) == 0)
                                && (orig_vbase == codebase)
                                && (orig_vbase < primary_toc.romhdr.physfirst);
        vbase_ok = in_dll_region || xip_codebase;
    }
    if (!vbase_ok) {
        LOG(GuestAdditions,
            "%s original vbase 0x%08X fails the vbase check (CE major=%u, codebase=0x%08X, dllfirst=0x%08X "
            "dlllast=0x%08X)\n",
            victim_name, orig_vbase, ce_major_, codebase, primary_toc.romhdr.dllfirst,
            primary_toc.romhdr.dlllast);
        emu_.Get<Fatal>().UserError("Guest Additions can't run on this ROM");
    }

    const size_t   nsec       = pe.Sections().size();
    const uint32_t foot_bytes = Align4(AlignRomPage(pe.ImageSize()) + L.size)
                              + uint32_t(nsec) * kO32RomSize;
    const bool     in_place   = (ce_major_ >= 6);

    const GuestStubHostSpan host = emu_.Get<GuestStubHost>().Pick(
        victim_name, orig_o32_pa, orig_objcnt, foot_bytes, in_place);
    const uint32_t host_va = host.va;
    const uint32_t host_pa = host.pa;

    uint32_t target_vbase = host_va;
    uint32_t run_base     = host_va;
    if (!in_place) {
        const uint32_t slot_base = codebase - orig_vbase;
        target_vbase = emu_.Get<GuestModulePlacer>().ComputeVbase(
            orig_vbase, slot_base, pe.ImageSize(), ce_major_, L.off_vbase,
            victim_name);
        run_base = target_vbase + slot_base;
    }

    std::vector<uint32_t> dataptr(nsec), realaddr(nsec), flags(nsec);
    for (size_t i = 0; i < nsec; ++i) {
        const auto& s = pe.Sections()[i];
        dataptr[i]  = host_va + s.rva;
        realaddr[i] = run_base + s.rva;
        flags[i] = in_place
            ? emu_.Get<GuestModulePlacer>().EffSectionFlags(s.flags)
            : (s.flags | kImgScnMemWrite | kImgScnMemShared);
    }

    const uint32_t e32_va = host_va + AlignRomPage(pe.ImageSize());
    const uint32_t o32_va = Align4(e32_va + L.size);

    std::vector<uint8_t> patched_bytes(pe.Bytes().begin(), pe.Bytes().end());
    const int32_t code_delta = int32_t(run_base) - int32_t(pe.ImageBase());
    uint32_t reloc_count = 0;
    uint32_t unhandled_relocs = 0;
    cerf::ce_image_relocator::ApplyRelocations(
        patched_bytes, pe, realaddr, code_delta, reloc_count, unhandled_relocs);
    LOG(GuestAdditions, "%s reloc code_delta=0x%08X patched=%u unhandled=%u\n",
        victim_name, uint32_t(code_delta), reloc_count, unhandled_relocs);
    if (unhandled_relocs > 0) {
        LOG(GuestAdditions, "%s stub has %u unhandled relocation entries\n", victim_name, unhandled_relocs);
        emu_.Get<Fatal>().UserError("Guest Additions can't run on this ROM");
    }

    emu_.Get<GuestAdditionsBinaries>().StampWindowBase(patched_bytes);

    /* Only CE2.11 needs this: it does not bind an injected ROM module's imports
       (CE3+ kernels rebind at load). Restricting to CE2 also avoids reading CE3+
       coredll, whose ROM sections may be compressed (garbage at injection time). */
    if (ce_major_ <= 2) {
        emu_.Get<CeImportBinder>().BindImports(patched_bytes, pe, L);
    }

    std::vector<uint8_t> foot(foot_bytes, 0);
    WriteE32Rom(foot.data() + (e32_va - host_va), pe, target_vbase, orig_subsysmaj, orig_subsysmin);
    WriteO32Array(foot.data() + (o32_va - host_va), pe, dataptr, realaddr, flags);
    WriteSectionBytes(foot, pe, patched_bytes);

    auto& image = emu_.Get<GuestXipImage>();
    image.Patch(host_pa, foot);

    const uint32_t entry_pa = pt.VaToPa(entry_kva);
    image.PatchWord(entry_pa + kTocOffNFileSize,  uint32_t(pe_bytes_size));
    image.PatchWord(entry_pa + kTocOffE32Offset,  e32_va);
    image.PatchWord(entry_pa + kTocOffO32Offset,  o32_va);
    image.PatchWord(entry_pa + kTocOffLoadOffset, host_va);

    LOG(GuestAdditions, "%s stub injected (%s, %s): idx=%zu entry_kva=0x%08X "
              "host VA=0x%08X PA=0x%08X used=0x%X avail=0x%X e32=0x%08X o32=0x%08X "
              "sections=%zu vbase=0x%08X run_base=0x%08X\n",
        victim_name, host.squat ? "squat" : "band", in_place ? "in-place" : "copy",
        victim_idx, entry_kva, host_va, host_pa, foot_bytes, host.avail,
        e32_va, o32_va, nsec, target_vbase, run_base);

    if (in_place) image.RestoreOnEveryReset(host_pa, foot_bytes);
    return true;
}

}  /* namespace */

REGISTER_SERVICE(GuestAdditionsInjector);

}
