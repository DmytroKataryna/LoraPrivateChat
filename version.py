# Підставляє дані про збірку в макроси препроцесора.
# Викликається PlatformIO перед компіляцією через extra_scripts.

Import("env")

import subprocess
import datetime


def git(args, fallback="unknown"):
    """Повертає вивід git або fallback, якщо репозиторію чи git немає.
    Збірка з архіву без .git — цілком нормальний сценарій."""
    try:
        out = subprocess.check_output(
            ["git"] + args,
            stderr=subprocess.DEVNULL,
        )
        return out.decode("utf-8", "replace").strip()
    except Exception:
        return fallback


commit = git(["rev-parse", "--short", "HEAD"])
branch = git(["rev-parse", "--abbrev-ref", "HEAD"])

# Незакомічені зміни роблять hash оманливим: код у прошивці
# не збігається з тим, що лежить у репозиторії під цим номером
if git(["status", "--porcelain"], ""):
    commit += "-dirty"

# Дату беремо тут, а не з __DATE__: той макрос оновлюється лише
# коли конкретний .cpp перекомпілюється, тож легко отримати
# вчорашню дату в сьогоднішній збірці
built = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")

print("build %s (%s) %s" % (commit, branch, built))

# StringifyMacro сам розставляє екранування лапок так, щоб вони
# дожили до компілятора. Ручне -D X=\"...\" ламається по-різному
# на різних оболонках.
env.Append(CPPDEFINES=[
    ("BUILD_COMMIT", env.StringifyMacro(commit)),
    ("BUILD_BRANCH", env.StringifyMacro(branch)),
    ("BUILD_DATE", env.StringifyMacro(built)),
])
