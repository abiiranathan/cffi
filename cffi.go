// cffi.go
// This file contains the CFFI bindings for the recent_visits.h C header file.
// It is used to interface with the C code in the recent_visits.c file.
package cffi

// #include "recent_visits.h"
import "C"
import (
	"fmt"
	"time"
	"unsafe"
)

// VisitManager represents a manager for tracking recent visits.
type VisitManager struct {
	// Pointer to the C struct that manages recent visits
	ptr *C.VisitManager
}

// Visit represents a single visit and contains the visit ID, URL, text, and time of the visit.
type Visit struct {
	VisitID uint32
	URL     string
	Text    string
	Time    time.Time
}

// NewVisitManager creates a new VisitManager instance.
func NewVisitManager(file string, maxVisitsPerUser int) (*VisitManager, error) {
	// Convert Go string to C string
	cFile := C.CString(file)
	defer C.free(unsafe.Pointer(cFile))

	// Create a new VisitManager instance using the C function
	ptr := C.VisitManagerCreate(cFile, C.size_t(maxVisitsPerUser))
	if ptr == nil {
		return nil, fmt.Errorf("failed to create VisitManager")
	}
	return &VisitManager{ptr: ptr}, nil
}

// Close releases the resources of the VisitManager.
func (vm *VisitManager) Close() {
	if vm.ptr != nil {
		C.VisitManagerFree(vm.ptr)
		vm.ptr = nil
	}
}

// AddVisit adds a visit for a user.
func (vm *VisitManager) AddVisit(userID, visitID uint32, url, text string) bool {
	cURL := C.CString(url)
	defer C.free(unsafe.Pointer(cURL))
	cText := C.CString(text)
	defer C.free(unsafe.Pointer(cText))

	// Call the C function to add a visit
	return bool(C.VisitManagerAddVisit(vm.ptr, C.uint32_t(userID), C.uint32_t(visitID), cURL, cText))
}

// GetRecentVisits returns recent visits for a user.
func (vm *VisitManager) GetRecentVisits(userID uint32) ([]Visit, error) {
	var count C.size_t

	// The pointer is owned by the manager and must not be freed here.
	visitPtrs := C.VisitManagerGetRecentVisits(vm.ptr, C.uint32_t(userID), &count)
	if visitPtrs == nil || count == 0 {
		return nil, nil // No visits found
	}

	// Convert the C array into a Go slice of *C.Visit
	// Each visit is assumed to be a C.Visit struct in a contiguous block
	visitsC := (*[1 << 30]*C.Visit)(unsafe.Pointer(visitPtrs))[:count:count]

	// Convert each *C.Visit to a Go Visit
	visits := make([]Visit, count)
	for i := range visits {
		cVisit := visitsC[i]
		visits[i] = Visit{
			VisitID: uint32(cVisit.visit_id),
			URL:     C.GoString(cVisit.url),
			Text:    C.GoString(cVisit.text),
			Time:    time.Unix(int64(cVisit.time.tv_sec), int64(cVisit.time.tv_nsec)),
		}
	}

	return visits, nil
}

// DeleteVisits deletes the specified visits for a user.
func (vm *VisitManager) DeleteVisits(userID uint32, visitIDs []uint32) bool {
	if len(visitIDs) == 0 {
		return true
	}

	// Convert the visit IDs to a C array
	cVisitIDs := (*C.uint32_t)(unsafe.Pointer(&visitIDs[0]))
	return bool(C.VisitManagerDelete(vm.ptr, C.uint32_t(userID), cVisitIDs, C.size_t(len(visitIDs))))
}

// ClearUser clears all visits for a user.
func (vm *VisitManager) ClearUser(userID uint32) {
	C.VisitManagerClear(vm.ptr, C.uint32_t(userID))
}
