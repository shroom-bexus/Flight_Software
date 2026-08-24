Import("env")

import subprocess


def get_git_version():
    try:
        version = subprocess.check_output(
            [
                "git",
                "describe",
                "--tags",
                "--always",
                "--dirty"
            ],
            cwd=env.subst("$PROJECT_DIR"),
            text=True
        ).strip()

        return version

    except Exception:
        return "unknown"


git_version = get_git_version()

env.Append(
    CPPDEFINES=[
        ("FLIGHT_VERSION", '\\"{}\\"'.format(git_version))
    ]
)

print("Flight software version:", git_version)