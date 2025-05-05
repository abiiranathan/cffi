# cffi
---

# C Binary Serialization with FFI Bindings for Golang and Python

This project provides an example of C binary serialization with Foreign Function Interface (FFI) bindings for Golang and Python. The core functionality involves serializing and deserializing visit data and allows seamless integration with Golang and Python applications.

## Features

* **C Binary Serialization**: Serialize complex data structures in C to binary format for efficient storage and transmission.
* **FFI Bindings**:

  * **Python (CFFI)**: Interface with C functions and use C data structures directly in Python.
  * **Golang (CGo)**: Interface with C functions and data structures directly in Go.

## C Binary Serialization API

The C implementation supports the management of user visits, serializing and deserializing visit data to a file. The core data structure is `Visit`, and `VisitManager` is used to manage visits for each user.

### C Header File (`recent_visits.h`)

```c
#ifndef RECENT_VISITS_H
#define RECENT_VISITS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    uint32_t visit_id;     // The ID of the visit
    char* url;             // Pointer to null-terminated string
    char* text;            // Pointer to null-terminated string
    struct timespec time;  // Timestamp
} Visit;

typedef struct VisitManager VisitManager;

// Create and initialize a VisitManager.
// If path exists, deserialization is done to populate the state.
VisitManager* VisitManagerCreate(const char* path, size_t max_visits);

// Free memory allocated by visit manager.
void VisitManagerFree(VisitManager* manager);

// Add a visit for a user.
bool VisitManagerAddVisit(VisitManager* manager, uint32_t user_id, uint32_t visit_id, const char* url, const char* text);

// Get recent visits for a user.
Visit** VisitManagerGetRecentVisits(VisitManager* manager, uint32_t user_id, size_t* count);

// Delete visit IDs and re-serialize.
bool VisitManagerDelete(VisitManager* manager, uint32_t user_id, uint32_t* visitIds, size_t visit_count);

// Clear visits for a user.
void VisitManagerClear(VisitManager* manager, uint32_t user_id);

#endif /* RECENT_VISITS_H */
```

### C Implementation

The `serialization.c` file implements the functions to manage visits, serialize and deserialize data, and handle visit storage.

---

## Python FFI Bindings (Using CFFI)

The Python bindings use `ctypes` to interface with the C API and define the `Visit` and `VisitManager` classes for Python.

### Python Code (`recent_visits.py`)

```python
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

```

### Example Usage

1. **Add a Visit**: You can add a visit using the `add_visit` method by providing the `user_id`, `visit_id`, `url`, and `text`.
2. **Get Recent Visits**: Retrieve recent visits for a user using the `get_recent_visits` method.
3. **Delete Visits**: Delete specific visits for a user using the `delete_visits` method.
4. **Clear Visits**: You can clear all visits for a specific user using the `clear_user` method.

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
