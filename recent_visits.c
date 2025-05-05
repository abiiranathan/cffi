#include "recent_visits.h"
#include <assert.h>

// Internal structure to store visits per user
typedef struct {
    uint32_t user_id;
    Visit** visits;
    size_t visit_count;
    size_t capacity;
} UserVisits;

// Internal structure of the VisitManager
struct VisitManager {
    UserVisits** users;
    size_t user_count;
    size_t user_capacity;
    size_t max_visits;
    char* path;
};

// Helper function to find user entry or return NULL if not found
static UserVisits* find_user(VisitManager* manager, uint32_t user_id) {
    for (size_t i = 0; i < manager->user_count; i++) {
        if (manager->users[i]->user_id == user_id) {
            return manager->users[i];
        }
    }
    return NULL;
}

// Helper function to create and initialize a new user entry
static UserVisits* create_user(uint32_t user_id, size_t initial_capacity) {
    UserVisits* user = (UserVisits*)malloc(sizeof(UserVisits));
    if (!user) {
        return NULL;
    }

    user->visits = (Visit**)malloc(initial_capacity * sizeof(Visit*));
    if (!user->visits) {
        free(user);
        return NULL;
    }

    user->user_id     = user_id;
    user->visit_count = 0;
    user->capacity    = initial_capacity;

    return user;
}

// Helper function to free visit memory
static void free_visit(Visit* visit) {
    if (visit) {
        free(visit->url);
        free(visit->text);
        free(visit);
    }
}

// Helper function to free user and user visits memory.
static void free_user_visits(UserVisits* user) {
    if (user) {
        for (size_t i = 0; i < user->visit_count; i++) {
            free_visit(user->visits[i]);
        }
        free(user->visits);
        free(user);
    }
}

// Helper function to create a new visit
static Visit* create_visit(uint32_t visit_id, const char* url, const char* text) {
    Visit* visit = (Visit*)malloc(sizeof(Visit));
    if (!visit) {
        return NULL;
    }

    visit->visit_id = visit_id;

    visit->url = strdup(url);
    if (!visit->url) {
        free(visit);
        return NULL;
    }

    visit->text = strdup(text);
    if (!visit->text) {
        free(visit->url);
        free(visit);
        return NULL;
    }

    // Set current time
    timespec_get(&visit->time, TIME_UTC);
    // clock_gettime(CLOCK_REALTIME, &visit->time);
    return visit;
}

// Helper function for serialization
static void serialize_manager(VisitManager* manager) {
    FILE* file = fopen(manager->path, "wb");
    if (!file) {
        return;
    }

    // Write max_visits
    fwrite(&manager->max_visits, sizeof(size_t), 1, file);

    // Write user count
    fwrite(&manager->user_count, sizeof(size_t), 1, file);

    // Write each user
    for (size_t i = 0; i < manager->user_count; i++) {
        UserVisits* user = manager->users[i];

        // Write user ID
        fwrite(&user->user_id, sizeof(uint32_t), 1, file);

        // Write visit count
        fwrite(&user->visit_count, sizeof(size_t), 1, file);

        // Write each visit
        for (size_t j = 0; j < user->visit_count; j++) {
            Visit* visit = user->visits[j];

            // Write visit ID
            fwrite(&visit->visit_id, sizeof(uint32_t), 1, file);

            // Write URL length and URL
            size_t url_len = strlen(visit->url) + 1;
            fwrite(&url_len, sizeof(size_t), 1, file);
            fwrite(visit->url, 1, url_len, file);

            // Write text length and text
            size_t text_len = strlen(visit->text) + 1;
            fwrite(&text_len, sizeof(size_t), 1, file);
            fwrite(visit->text, 1, text_len, file);

            // Write timestamp
            fwrite(&visit->time, sizeof(struct timespec), 1, file);
        }
    }

    fclose(file);
}

// Helper function for deserialization
static VisitManager* deserialize_manager(const char* path, size_t max_visits) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    VisitManager* manager = (VisitManager*)malloc(sizeof(VisitManager));
    if (!manager) {
        fclose(file);
        return NULL;
    }

    manager->path = strdup(path);
    if (!manager->path) {
        free(manager);
        fclose(file);
        return NULL;
    }

    // Read max_visits from file but use the provided value
    size_t stored_max_visits;
    if (fread(&stored_max_visits, sizeof(size_t), 1, file) != 1) {
        free(manager->path);
        free(manager);
        fclose(file);
        return NULL;
    }
    manager->max_visits = max_visits;

    // Read user count
    if (fread(&manager->user_count, sizeof(size_t), 1, file) != 1) {
        free(manager->path);
        free(manager);
        fclose(file);
        return NULL;
    }

    // Allocate users array
    manager->user_capacity = manager->user_count > 0 ? manager->user_count : 10;
    manager->users         = (UserVisits**)malloc(manager->user_capacity * sizeof(UserVisits*));
    if (!manager->users) {
        free(manager->path);
        free(manager);
        fclose(file);
        return NULL;
    }

    // Read each user
    for (size_t i = 0; i < manager->user_count; i++) {
        uint32_t user_id;
        if (fread(&user_id, sizeof(uint32_t), 1, file) != 1) {
            goto cleanup;
        }

        size_t visit_count;
        if (fread(&visit_count, sizeof(size_t), 1, file) != 1) {
            goto cleanup;
        }

        // Create user
        UserVisits* user = create_user(user_id, visit_count > 0 ? visit_count : 10);
        if (!user) {
            goto cleanup;
        }

        manager->users[i] = user;

        // Read each visit
        for (size_t j = 0; j < visit_count; j++) {
            Visit* visit = (Visit*)malloc(sizeof(Visit));
            if (!visit) {
                goto cleanup;
            }

            // Read visit ID
            if (fread(&visit->visit_id, sizeof(uint32_t), 1, file) != 1) {
                free(visit);
                goto cleanup;
            }

            // Read URL
            size_t url_len;
            if (fread(&url_len, sizeof(size_t), 1, file) != 1) {
                free(visit);
                goto cleanup;
            }

            visit->url = (char*)malloc(url_len);
            if (!visit->url) {
                free(visit);
                goto cleanup;
            }

            if (fread(visit->url, 1, url_len, file) != url_len) {
                free(visit->url);
                free(visit);
                goto cleanup;
            }

            // Read text
            size_t text_len;
            if (fread(&text_len, sizeof(size_t), 1, file) != 1) {
                free(visit->url);
                free(visit);
                goto cleanup;
            }

            visit->text = (char*)malloc(text_len);
            if (!visit->text) {
                free(visit->url);
                free(visit);
                goto cleanup;
            }

            if (fread(visit->text, 1, text_len, file) != text_len) {
                free(visit->text);
                free(visit->url);
                free(visit);
                goto cleanup;
            }

            // Read timestamp
            if (fread(&visit->time, sizeof(struct timespec), 1, file) != 1) {
                free(visit->text);
                free(visit->url);
                free(visit);
                goto cleanup;
            }

            // Add visit to user
            if (j < max_visits) {
                user->visits[j] = visit;
                user->visit_count++;
            } else {
                // Skip if beyond max_visits
                free_visit(visit);
            }
        }
    }

    fclose(file);
    return manager;

cleanup:
    // Handle deserialization failure
    for (size_t i = 0; i < manager->user_count; i++) {
        if (manager->users[i]) {
            free_user_visits(manager->users[i]);
        }
    }

    free(manager->users);
    free(manager->path);
    free(manager);
    fclose(file);
    return NULL;
}

VisitManager* VisitManagerCreate(const char* path, size_t max_visits) {
    VisitManager* manager = NULL;

    // Try to deserialize if file exists
    FILE* file = fopen(path, "rb");
    if (file) {
        fclose(file);
        manager = deserialize_manager(path, max_visits);
    }

    // Create new manager if deserialization failed or file doesn't exist
    if (!manager) {
        manager = (VisitManager*)malloc(sizeof(VisitManager));
        if (!manager) {
            return NULL;
        }

        manager->path = strdup(path);
        if (!manager->path) {
            free(manager);
            return NULL;
        }

        manager->max_visits    = max_visits;
        manager->user_count    = 0;
        manager->user_capacity = 10;  // Initial capacity

        manager->users = (UserVisits**)malloc(manager->user_capacity * sizeof(UserVisits*));
        if (!manager->users) {
            free(manager->path);
            free(manager);
            return NULL;
        }
    }

    return manager;
}

void VisitManagerFree(VisitManager* manager) {
    if (!manager) {
        return;
    }

    // Free all user visits
    for (size_t i = 0; i < manager->user_count; i++) {
        free_user_visits(manager->users[i]);
    }

    // Free remaining manager resources
    free(manager->users);
    free(manager->path);
    free(manager);
}

bool VisitManagerAddVisit(VisitManager* manager, uint32_t user_id, uint32_t visit_id, const char* url,
                          const char* text) {
    if (!manager || !url || !text) {
        return false;
    }

    // Find or create user entry
    UserVisits* user = find_user(manager, user_id);
    if (!user) {
        // User not found, create new user entry
        if (manager->user_count >= manager->user_capacity) {
            // Resize users array if needed
            size_t new_capacity    = manager->user_capacity * 2;
            UserVisits** new_users = (UserVisits**)realloc(manager->users, new_capacity * sizeof(UserVisits*));
            if (!new_users) {
                return false;
            }
            manager->users         = new_users;
            manager->user_capacity = new_capacity;
        }

        user = create_user(user_id, manager->max_visits);
        if (!user) {
            return false;
        }

        manager->users[manager->user_count++] = user;
    }

    // If visit already exists, ignore it.
    for (size_t i = 0; i < user->visit_count; i++) {
        if (user->visits[i]->visit_id == visit_id) {
            fprintf(stderr, "A visit with ID: %u already exists\n", visit_id);
            return true;  // No need to report failure
        }
    }

    // Create new visit
    Visit* visit = create_visit(visit_id, url, text);
    if (!visit) {
        return false;
    }

    // Check if we need to make room (remove oldest visit)
    if (user->visit_count >= manager->max_visits) {
        // Find the oldest visit (assuming visits are not necessarily in order)
        size_t oldest_idx           = 0;
        struct timespec oldest_time = user->visits[0]->time;

        for (size_t i = 1; i < user->visit_count; i++) {
            if (user->visits[i]->time.tv_sec < oldest_time.tv_sec ||
                (user->visits[i]->time.tv_sec == oldest_time.tv_sec &&
                 user->visits[i]->time.tv_nsec < oldest_time.tv_nsec)) {
                oldest_idx  = i;
                oldest_time = user->visits[i]->time;
            }
        }

        // Free oldest visit
        free_visit(user->visits[oldest_idx]);

        // Move the last visit to the removed position if not removing the last one
        if (oldest_idx < user->visit_count - 1) {
            user->visits[oldest_idx] = user->visits[user->visit_count - 1];
        }

        user->visit_count--;
    }

    // Check if we need to resize visits array
    if (user->visit_count >= user->capacity) {
        size_t new_capacity = user->capacity * 2;
        if (new_capacity > manager->max_visits) {
            new_capacity = manager->max_visits;
        }

        Visit** new_visits = (Visit**)realloc(user->visits, new_capacity * sizeof(Visit*));
        if (!new_visits) {
            free_visit(visit);
            return false;
        }

        user->visits   = new_visits;
        user->capacity = new_capacity;
    }

    // Add the new visit
    user->visits[user->visit_count++] = visit;

    // Serialize changes to disk
    serialize_manager(manager);

    return true;
}

// Comparison function for qsort
static int compare_visits(const void* a, const void* b) {
    struct timespec* time1 = &(((Visit*)a)->time);
    struct timespec* time2 = &(((Visit*)b)->time);

    // Compare timestamps (newer first)
    if (time1->tv_sec > time2->tv_sec) {
        return -1;  // time1 is newer, so it should come first
    } else if (time1->tv_sec < time2->tv_sec) {
        return 1;  // time2 is newer, so it should come first
    } else {
        if (time1->tv_nsec > time2->tv_nsec) {
            return -1;  // time1 is newer
        } else if (time1->tv_nsec < time2->tv_nsec) {
            return 1;  // time2 is newer
        } else {
            return 0;  // times are equal
        }
    }
}

// Function to sort the visits by time (newest first)
void sort_visits(Visit* visits[], size_t visit_count) {
    qsort(visits, visit_count, sizeof(Visit*), compare_visits);
}

Visit** VisitManagerGetRecentVisits(VisitManager* manager, uint32_t user_id, size_t* count) {
    if (!manager || !count) {
        return NULL;
    }

    // Find user
    UserVisits* user = find_user(manager, user_id);
    if (!user) {
        *count = 0;
        return NULL;
    }

    // Sort visits by time (newest first)
    sort_visits(user->visits, user->visit_count);
    *count = user->visit_count;
    return user->visits;
}

bool VisitManagerDelete(VisitManager* manager, uint32_t user_id, uint32_t* visitIds, size_t visit_count) {
    if (!manager || !visitIds || visit_count == 0) {
        return false;
    }

    // Find user
    UserVisits* user = find_user(manager, user_id);
    if (!user) {
        return false;
    }

    bool found_any = false;

    // For each visit ID to delete
    for (size_t i = 0; i < visit_count; i++) {
        uint32_t id_to_delete = visitIds[i];

        // Find the visit with this ID
        for (size_t j = 0; j < user->visit_count; j++) {
            if (user->visits[j]->visit_id == id_to_delete) {
                // Free the visit
                free_visit(user->visits[j]);

                // Replace with the last visit (unless this is the last one)
                // Swap-and-pop strategy.
                if (j < user->visit_count - 1) {
                    user->visits[j] = user->visits[user->visit_count - 1];
                    j--;  // Recheck this position since we moved a new visit here
                }

                user->visit_count--;
                found_any = true;
                break;
            }
        }
    }

    if (found_any) {
        // Serialize changes to disk
        serialize_manager(manager);
        return true;
    }

    return false;
}

void VisitManagerClear(VisitManager* manager, uint32_t user_id) {
    if (!manager) {
        return;
    }

    // Find user
    UserVisits* user = find_user(manager, user_id);
    if (!user) {
        return;
    }

    // Free all visits
    for (size_t i = 0; i < user->visit_count; i++) {
        free_visit(user->visits[i]);
    }

    // Reset count
    user->visit_count = 0;

    // Serialize changes to disk
    serialize_manager(manager);
}
