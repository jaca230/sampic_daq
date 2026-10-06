"""Common types and path helpers for SAMPIC ODB profiles."""

from abc import ABC, abstractmethod
from argparse import ArgumentParser, Namespace
from dataclasses import dataclass
from typing import Any, Sequence


@dataclass(frozen=True)
class OdbWrite:
    """One documented ODB value written by a profile."""

    path: str
    value: Any
    description: str


class OdbProfile(ABC):
    """Base class implemented by each discoverable ODB profile."""

    name: str
    description: str

    def configure_parser(self, parser: ArgumentParser) -> None:
        """Add profile-specific command-line arguments."""

    @abstractmethod
    def build_writes(self, arguments: Namespace) -> Sequence[OdbWrite]:
        """Return the complete set of writes for this invocation."""


def settings_root(frontend_index: int) -> str:
    """Return the canonical Settings root for one SAMPIC frontend."""
    if frontend_index < 0 or frontend_index > 99:
        raise ValueError("frontend index must be in [0, 99]")
    return f"/Equipment/SAMPIC {frontend_index:02d}/Settings"


def correction_writes(root: str) -> list[OdbWrite]:
    """Enable every vendor correction supported by loaded calibration data."""
    correction_root = f"{root}/Crate/correction_levels"
    return [
        OdbWrite(
            f"{correction_root}/adc_linearity",
            True,
            "Apply loaded ADC-linearity calibration to waveform samples.",
        ),
        OdbWrite(
            f"{correction_root}/time_inl",
            True,
            "Apply loaded time-INL calibration to sample timing.",
        ),
        OdbWrite(
            f"{correction_root}/residual_pedestal",
            True,
            "Apply residual-pedestal calibration when available.",
        ),
    ]


def parse_index_selection(
    raw: str,
    upper_bound: int,
    label: str,
) -> list[int]:
    """Parse 'all' or a comma-separated set of hardware indices."""
    if raw.lower() in {"all", "*"}:
        return list(range(upper_bound))

    try:
        values = sorted(
            {int(item.strip()) for item in raw.split(",") if item.strip()}
        )
    except ValueError as error:
        raise ValueError(f"invalid {label} selection: {raw}") from error

    if not values:
        raise ValueError(f"no {label} indices provided")
    if values[0] < 0 or values[-1] >= upper_bound:
        raise ValueError(
            f"{label} indices must be in [0, {upper_bound - 1}]"
        )
    return values
