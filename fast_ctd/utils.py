"""Utility functions for fast_ctd."""

from pathlib import Path
from typing import TypeVar

T = TypeVar("T")


def none_guard(
    value: T | None,
    default: T,
) -> T:
    """Return the default value if the input is None, otherwise return the input value."""
    return default if value is None else value


def validate_file_extension(
    file_path: Path,
    expected_extension: str | tuple[str, ...],
) -> None:
    """Validate the file extension of a given file path."""
    expected_extension = (
        (expected_extension,)
        if isinstance(expected_extension, str)
        else expected_extension
    )
    if file_path.suffix not in expected_extension:
        raise ValueError(
            f"File must be one of {expected_extension}, but got {file_path.suffix}",
        )


def validate_file_exists(
    file_path: Path,
    suffix_stmt: str = "Are you in the right directory?",
) -> None:
    """Validate if a file exists."""
    if not file_path.exists():
        raise FileNotFoundError(f"'{file_path}' does not exist. {suffix_stmt}")
