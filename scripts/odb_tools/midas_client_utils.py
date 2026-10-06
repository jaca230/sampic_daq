"""Shared MIDAS Python client loading for command-line ODB tools."""

import os
import sys
from pathlib import Path


def create_midas_client(client_name: str):
    """Create a MIDAS client after locating MIDASSYS/python when necessary."""
    midas_root = os.environ.get("MIDASSYS")
    if midas_root:
        python_directory = str(Path(midas_root) / "python")
        if python_directory not in sys.path:
            sys.path.insert(0, python_directory)

    try:
        import midas.client
    except ModuleNotFoundError as error:
        raise RuntimeError(
            "cannot import midas.client; source the MIDAS environment or set "
            "MIDASSYS"
        ) from error

    return midas.client.MidasClient(client_name)
