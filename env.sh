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


export TOOLCHAIN_PATH
export PATH="${PATH}:${TOOLCHAIN_PATH}"