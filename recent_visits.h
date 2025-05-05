
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

// The visit manager tracks most recent visits per user.
// It uses a map where the key is the user ID. (uint32_t) and value is a dynamic array
// of visits.
// The path is the path where the data is serialized and desrialized from disk.
typedef struct VisitManager VisitManager;

// Create and initialize a VisitManager.
// If path, exists deserialization is done to populate the state.
VisitManager* VisitManagerCreate(const char* path, size_t max_visits);

// Free memory allocated by visit manager.
void VisitManagerFree(VisitManager* manager);

// Add a visit for a user
bool VisitManagerAddVisit(VisitManager* manager, uint32_t user_id, uint32_t visit_id, const char* url,
                          const char* text);

// Get recent visits for a user. The manager owns the memory pointed to by visits.
Visit** VisitManagerGetRecentVisits(VisitManager* manager, uint32_t user_id, size_t* count);

// Delete visit IDs and re-serialize.
bool VisitManagerDelete(VisitManager* manager, uint32_t user_id, uint32_t* visitIds, size_t visit_count);

// Clear visits for a user
void VisitManagerClear(VisitManager* manager, uint32_t user_id);

#endif /* RECENT_VISITS_H */
