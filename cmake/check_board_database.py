import json
import sys
from pathlib import Path


def picker_errors(db: dict) -> list:
    parts = {p["id"]: p for p in db["parts"]}
    errors = []
    for param in db["board_config_params"]:
        if not param.get("part_picker"):
            continue
        for value in param["enum_values"]:
            part_id = value.get("part_id")
            if part_id is None:
                continue
            part = parts.get(part_id)
            if part is None:
                errors.append(f"{param['id']} offers {part_id}, which is not a part")
            elif part["generic_part_id"] != param["generic_part_id"]:
                errors.append(f"{param['id']} offers {part_id}, a {part['generic_part_id']} part")
            elif "connections" in part:
                errors.append(f"{param['id']} offers {part_id}, which has connections")
    return errors


def clamp_errors(db: dict) -> list:
    return [f"{p['id']} sets clamp_viewport with no width and height" for p in db["parts"]
            if p.get("clamp_viewport") and not ("width" in p and "height" in p)]


def main(path: str) -> int:
    db = json.loads(Path(path).read_text(encoding="utf-8"))
    errors = picker_errors(db) + clamp_errors(db)
    for error in errors:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
