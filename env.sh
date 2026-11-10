#!/usr/bin/env bash
SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )"
TOOLCHAIN_PATH="${SCRIPT_DIR}/toolchain"
PYTHON_ENV="${SCRIPT_DIR}/pydevs"

if [ ! -d "${PYTHON_ENV}" ]; then
    python3 -m venv "${PYTHON_ENV}"   
    source "${PYTHON_ENV}/bin/activate" 
    pip install -r ${SCRIPT_DIR}/packages.txt
else
    if [[ "$OSTYPE" == "cygwin" ]]; then
        source "${PYTHON_ENV}/Scripts/activate"
    else
        source "${PYTHON_ENV}/bin/activate"
    fi
fi
export ALPACA_API_KEY="PKZ2HPI2RCS42PQRGM4IBWNKVB"
export ALPACA_API_SECRET="2QZNpxG3HWtB6d1Zo64FWR6tZkbMaGeqzkkHjEcYhHF9"

export TOOLCHAIN_PATH

if [[ "$OSTYPE" == "msys" ]]; then
    export PATH="${PATH}:${TOOLCHAIN_PATH}:/c/Users/rafae/AppData/Local/Microsoft/WindowsApps/PythonSoftwareFoundation.Python.3.12_qbz5n2kfra8p0"
else
    export PATH="${PATH}:${TOOLCHAIN_PATH}"
fi

export ENSEMBLE_DSN="TUXEDOCAT"
export ENSEMBLE_UID="sa"
export ENSEMBLE_PWD="YourStrongPass123!"