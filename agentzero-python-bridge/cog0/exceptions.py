"""Exceptions for the cog0 Python bridge."""


class Cog0Error(Exception):
    """Base error for cog0 Python bindings."""


class Cog0LibraryNotFound(Cog0Error):
    """Raised when libcog0_capi cannot be located."""


class Cog0RuntimeError(Cog0Error):
    """Raised when a C API call fails at runtime."""
