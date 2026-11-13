import os

forbidden_exts = ['.apk', '.so', '.ogg', '.wav', '.plist', '.pvr', '.ccz', '.o', '.elf', '.pbp', '.tex', '.asm', '.bin', '.dat', '.sfo']
with open('/tmp/git_dryrun.txt') as f:
    lines = [line.strip() for line in f if line.strip()]

violations = []
for l in lines:
    filename = l.split(maxsplit=1)[1].strip("'")
    _, ext = os.path.splitext(filename)
    if ext.lower() in forbidden_exts:
        violations.append((filename, 'forbidden extension: ' + ext))
    if 'original_apk' in filename or 'assets/' in filename or 'assets_converted' in filename or 'data/' in filename:
        violations.append((filename, 'forbidden path'))

print(f"Total files staged in dry-run: {len(lines)}")
print(f"Violations found: {len(violations)}")
if violations:
    for v in violations:
        print("VIOLATION:", v)
else:
    print("ALL FILES 100% CLEAN AND COMPLIANT!")
