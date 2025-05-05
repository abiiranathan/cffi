from ctypes import *
import os
from datetime import datetime, timezone
from dataclasses import dataclass
from typing import List

# Load the shared library
lib_path = os.path.join(os.path.dirname(__file__), 'librv.so')
lib = CDLL(lib_path)

class timespec(Structure):
    _fields_ = [("tv_sec", c_long), ("tv_nsec", c_long)]

class CVisit(Structure):
    _fields_ = [
        ("visit_id", c_uint32),
        ("url", c_char_p),
        ("text", c_char_p),
        ("time", timespec)
    ]

@dataclass
class Visit:
    visit_id: int
    url: str
    text: str
    time: datetime
    
    @classmethod
    def from_cvisit(cls, cvisit: CVisit) -> 'Visit':
        return cls(
            visit_id=cvisit.visit_id,
            url=cvisit.url.decode('utf-8') if cvisit.url else "",
            text=cvisit.text.decode('utf-8') if cvisit.text else "",
            time=datetime.fromtimestamp(
                cvisit.time.tv_sec + cvisit.time.tv_nsec / 1e9,
                tz=timezone.utc
            )
        )
    
    def to_cvisit(self) -> CVisit:
        cvisit = CVisit()
        cvisit.visit_id = self.visit_id
        cvisit.url = self.url.encode('utf-8') if self.url else None
        cvisit.text = self.text.encode('utf-8') if self.text else None

        ts = self.time.timestamp()
        cvisit.time.tv_sec = int(ts)
        cvisit.time.tv_nsec = int((ts - int(ts)) * 1e9)
        return cvisit

# Function prototypes from the C API
lib.VisitManagerCreate.restype = c_void_p
lib.VisitManagerCreate.argtypes = [c_char_p, c_size_t]

lib.VisitManagerFree.argtypes = [c_void_p]

lib.VisitManagerAddVisit.argtypes = [
    c_void_p, c_uint32, c_uint32, c_char_p, c_char_p
]
lib.VisitManagerAddVisit.restype = c_bool

lib.VisitManagerGetRecentVisits.restype = POINTER(POINTER(CVisit))
lib.VisitManagerGetRecentVisits.argtypes = [c_void_p, c_uint32, POINTER(c_size_t)]

lib.VisitManagerDelete.argtypes = [ c_void_p, c_uint32, POINTER(c_uint32), c_size_t]
lib.VisitManagerDelete.restype = c_bool

lib.VisitManagerClear.argtypes = [c_void_p, c_uint32]

class VisitManager:
    def __init__(self, path: str, max_visits: int=10):
        self._path = path.encode('utf-8')
        self._max_visits = max_visits
        self._ptr = lib.VisitManagerCreate(self._path, self._max_visits)
        if not self._ptr:
            raise RuntimeError("Failed to create VisitManager")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def close(self):
        if getattr(self, '_ptr', None):
            lib.VisitManagerFree(self._ptr)
            self._ptr = None

    def add_visit(self, user_id: int, visit_id: int, url: str, text: str) -> bool:
        return lib.VisitManagerAddVisit(
            self._ptr,
            c_uint32(user_id),
            c_uint32(visit_id),
            url.encode('utf-8'),
            text.encode('utf-8')
        )

    def get_recent_visits(self, user_id: int) -> List[Visit]:
        count = c_size_t(0)
        visits_ptr_ptr = lib.VisitManagerGetRecentVisits(self._ptr, user_id, byref(count))

        if not visits_ptr_ptr or count.value == 0:
            return []

        visits = []
        for i in range(count.value):
            visit_ptr = visits_ptr_ptr[i]
            if not visit_ptr:
                continue  # skip null pointers
            visits.append(Visit.from_cvisit(visit_ptr.contents))
        return visits
    
    def delete_visits(self, user_id: int, visit_ids: List[int]) -> bool:
        if not visit_ids:
            return True
        ids_array = (c_uint32 * len(visit_ids))(*visit_ids)
        return lib.VisitManagerDelete(self._ptr, c_uint32(user_id), ids_array, len(visit_ids))

    def clear_user(self, user_id: int):
        lib.VisitManagerClear(self._ptr, c_uint32(user_id))


if __name__ == "__main__":
    import sys

    filename = sys.argv[1]
    with VisitManager(filename, 10) as vm:
        vm.add_visit(1, 101, "https://example.com", "Example Site")
        vm.add_visit(1, 102, "https://test.com", "Test Site")

        visits = vm.get_recent_visits(1)
        for v in visits:
            print(f"Visit ID: {v.visit_id}, URL: {v.url}, Text: {v.text}, Time: {v.time}")

        vm.delete_visits(1, [101])

        print("=======================")
        visits = vm.get_recent_visits(1)
        for v in visits:
            print(f"Visit ID: {v.visit_id}, URL: {v.url}, Text: {v.text}, Time: {v.time}")
            