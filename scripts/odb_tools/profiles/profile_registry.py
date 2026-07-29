"""Automatic discovery for isolated ODB profile modules."""

import importlib
import pkgutil
from typing import Dict

from profiles.profile_definition import OdbProfile


_INFRASTRUCTURE_MODULES = {
    "apply_profile",
    "profile_definition",
    "profile_registry",
    "reset_sampic_odb",
}


def discover_profiles() -> Dict[str, OdbProfile]:
    """Import profile modules and return their exported PROFILE objects."""
    package = importlib.import_module("profiles")
    profiles: Dict[str, OdbProfile] = {}

    for module_info in pkgutil.iter_modules(package.__path__):
        if (
            module_info.name.startswith("_")
            or module_info.name in _INFRASTRUCTURE_MODULES
        ):
            continue

        module = importlib.import_module(
            f"{package.__name__}.{module_info.name}"
        )
        profile = getattr(module, "PROFILE", None)
        if profile is None:
            continue
        if not isinstance(profile, OdbProfile):
            raise TypeError(
                f"{module.__name__}.PROFILE must inherit OdbProfile"
            )
        if not profile.name:
            raise ValueError(f"{module.__name__}.PROFILE has no name")
        if profile.name in profiles:
            raise ValueError(f"duplicate ODB profile '{profile.name}'")
        profiles[profile.name] = profile

    return dict(sorted(profiles.items()))
