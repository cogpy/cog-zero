# cython: language_level=3
"""
Cython bindings for the cog0 C API (include/cog0/cog0_capi.h).

Built optionally by agentzero-python-bridge when Cython is installed.
"""

from libc.stdlib cimport free
from libc.stddef cimport size_t

cdef extern from "cog0/cog0_capi.h":
    ctypedef struct cog0_agent_s
    ctypedef cog0_agent_s* cog0_agent_t

    cog0_agent_t cog0_agent_create(const char* name,
                                   int cycle_interval_ms,
                                   size_t max_tasks_per_cycle)
    void cog0_agent_free(cog0_agent_t agent)
    int cog0_agent_set_goal(cog0_agent_t agent,
                            const char* name,
                            const char* desc,
                            double priority)
    void cog0_agent_add_percept(cog0_agent_t agent,
                                const char* source,
                                const char* content,
                                double salience)
    void cog0_agent_run_cycles(cog0_agent_t agent, size_t n)
    void cog0_agent_start(cog0_agent_t agent)
    void cog0_agent_stop(cog0_agent_t agent)
    int cog0_agent_is_running(cog0_agent_t agent)
    size_t cog0_agent_cycle_count(cog0_agent_t agent)
    size_t cog0_agent_atom_count(cog0_agent_t agent)
    char* cog0_agent_status_report(cog0_agent_t agent)
    int cog0_agent_has_concept(cog0_agent_t agent, const char* name)
    void cog0_agent_add_concept(cog0_agent_t agent, const char* name)
    const char* cog0_version()


class Cog0Error(Exception):
    pass


class Cog0RuntimeError(Cog0Error):
    pass


def version():
    cdef const char* v = cog0_version()
    if v == NULL:
        return "unknown"
    return v.decode("utf-8")


cdef class Agent:
    cdef cog0_agent_t _handle

    def __cinit__(self, str name="cog0-agent", int cycle_interval_ms=10,
                  size_t max_tasks_per_cycle=10):
        cdef bytes bname = name.encode("utf-8")
        self._handle = cog0_agent_create(bname, cycle_interval_ms, max_tasks_per_cycle)
        if self._handle == NULL:
            raise Cog0RuntimeError("cog0_agent_create failed")

    def __dealloc__(self):
        if self._handle != NULL:
            cog0_agent_free(self._handle)
            self._handle = NULL

    def close(self):
        if self._handle != NULL:
            cog0_agent_free(self._handle)
            self._handle = NULL

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def set_goal(self, str name, str description="", double priority=0.5):
        cdef bytes bn = name.encode("utf-8")
        cdef bytes bd = description.encode("utf-8")
        if not cog0_agent_set_goal(self._handle, bn, bd, priority):
            raise Cog0RuntimeError(f"set_goal({name!r}) failed")

    def add_percept(self, str source, str content, double salience=0.5):
        cdef bytes bs = source.encode("utf-8")
        cdef bytes bc = content.encode("utf-8")
        cog0_agent_add_percept(self._handle, bs, bc, salience)

    def run_cycles(self, size_t n=1):
        cog0_agent_run_cycles(self._handle, n)

    def start(self):
        cog0_agent_start(self._handle)

    def stop(self):
        cog0_agent_stop(self._handle)

    @property
    def is_running(self):
        return bool(cog0_agent_is_running(self._handle))

    @property
    def cycle_count(self):
        return int(cog0_agent_cycle_count(self._handle))

    @property
    def atom_count(self):
        return int(cog0_agent_atom_count(self._handle))

    def status_report(self):
        cdef char* s = cog0_agent_status_report(self._handle)
        if s == NULL:
            return ""
        try:
            return s.decode("utf-8")
        finally:
            free(s)

    def has_concept(self, str name):
        cdef bytes bn = name.encode("utf-8")
        return bool(cog0_agent_has_concept(self._handle, bn))

    def add_concept(self, str name):
        cdef bytes bn = name.encode("utf-8")
        cog0_agent_add_concept(self._handle, bn)
