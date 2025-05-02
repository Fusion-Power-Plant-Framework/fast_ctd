import logging
from typing import Any

logger = logging.getLogger("fast_ctd")


def log_info(msg: str, *, more_info: dict[str, Any] | None = None) -> None:
    """Log message at INFO level."""
    if more_info:
        for key, value in more_info.items():
            msg += f"\n {key}: {value}"
    logger.info(msg)


def log_warn(msg: str, *, more_info: dict[str, Any] | None = None) -> None:
    """Log message at WARNING level."""
    if more_info:
        for key, value in more_info.items():
            msg += f"\n {key}: {value}"
    logger.warning(msg)


def config_basic_logging() -> None:
    """Configure basic logging."""
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )
