#!/usr/bin/env bash
SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )"
TOOLCHAIN_PATH="${SCRIPT_DIR}/toolchain"
PYTHON_ENV="${SCRIPT_DIR}/pydevs"

if [ ! -d "${PYTHON_ENV}" ]; then
    python3 -m venv "${PYTHON_ENV}"   
    source "${PYTHON_ENV}/bin/activate" 
    pip install -r ${SCRIPT_DIR}/packages.txt
else
    source "${PYTHON_ENV}/bin/activate"
fi
export ALPACA_API_KEY="PKZ2HPI2RCS42PQRGM4IBWNKVB"
export ALPACA_API_SECRET="2QZNpxG3HWtB6d1Zo64FWR6tZkbMaGeqzkkHjEcYhHF9"

export TOOLCHAIN_PATH
export PATH="${PATH}:${TOOLCHAIN_PATH}"
export ENSEMBLE_DSN="TUXEDOCAT"
export ENSEMBLE_UID="sa"
export ENSEMBLE_PWD="YourStrongPass123!"