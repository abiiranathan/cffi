#include <assert.h>
#include <stdio.h>
#include "recent_visits.h"

// Helper function to print a visit
void print_visit(Visit* visit) {
    printf("  Visit ID: %u\n", visit->visit_id);
    printf("  URL: %s\n", visit->url);
    printf("  Text: %s\n", visit->text);
    printf("  Time: %ld.%09ld\n", (long)visit->time.tv_sec, visit->time.tv_nsec);
    printf("\n");
}

// Helper function to print all visits for a user
void print_user_visits(VisitManager* manager, uint32_t user_id) {
    size_t count;
    Visit** visits = VisitManagerGetRecentVisits(manager, user_id, &count);

    printf("User %u has %zu visits:\n", user_id, count);
    if (count == 0) {
        printf("  No visits found.\n\n");
        return;
    }

    for (size_t i = 0; i < count; i++) {
        printf("Visit %zu:\n", i + 1);
        print_visit(visits[i]);
    }
}

// Test basic functionality
void test_basic(const char* test_file) {
    printf("\n=== BASIC TEST ===\n");

    // Create manager with maximum 5 visits per user
    printf("Creating visit manager...\n");
    VisitManager* manager = VisitManagerCreate(test_file, 5);
    assert(manager != NULL);

    // Add visits for user 1
    printf("Adding visits for user 1...\n");
    assert(VisitManagerAddVisit(manager, 1, 101, "https://example.com/1", "Example 1"));
    assert(VisitManagerAddVisit(manager, 1, 102, "https://example.com/2", "Example 2"));
    assert(VisitManagerAddVisit(manager, 1, 103, "https://example.com/3", "Example 3"));

    // Print visits for user 1
    print_user_visits(manager, 1);

    // Add visits for user 2
    printf("Adding visits for user 2...\n");
    assert(VisitManagerAddVisit(manager, 2, 201, "https://example.org/1", "Org 1"));
    assert(VisitManagerAddVisit(manager, 2, 202, "https://example.org/2", "Org 2"));

    // Print visits for user 2
    print_user_visits(manager, 2);

    // Delete a visit for user 1
    printf("Deleting visit 102 for user 1...\n");
    uint32_t to_delete[] = {102};
    assert(VisitManagerDelete(manager, 1, to_delete, 1));

    // Print visits for user 1 after deletion
    print_user_visits(manager, 1);

    // Clean up
    printf("Freeing visit manager...\n");
    VisitManagerFree(manager);
    printf("Basic test completed.\n");
}

// Test maximum visits limit
void test_max_visits(const char* test_file) {
    printf("\n=== MAX VISITS TEST ===\n");

    // Create manager with maximum 3 visits per user
    printf("Creating visit manager with max 3 visits...\n");
    VisitManager* manager = VisitManagerCreate(test_file, 3);
    assert(manager != NULL);

    // Add more than max visits for user 3
    printf("Adding 5 visits for user 3 (max is 3)...\n");
    assert(VisitManagerAddVisit(manager, 3, 301, "https://example.net/1", "Net 1"));
    // Sleep a bit to ensure different timestamps
    struct timespec sleep_time = {0, 100000000};  // 100 milliseconds
    nanosleep(&sleep_time, NULL);

    assert(VisitManagerAddVisit(manager, 3, 302, "https://example.net/2", "Net 2"));
    nanosleep(&sleep_time, NULL);

    assert(VisitManagerAddVisit(manager, 3, 303, "https://example.net/3", "Net 3"));
    nanosleep(&sleep_time, NULL);

    assert(VisitManagerAddVisit(manager, 3, 304, "https://example.net/4", "Net 4"));
    nanosleep(&sleep_time, NULL);

    assert(VisitManagerAddVisit(manager, 3, 305, "https://example.net/5", "Net 5"));

    // Print visits for user 3 (should only have the 3 most recent)
    print_user_visits(manager, 3);

    // Clean up
    printf("Freeing visit manager...\n");
    VisitManagerFree(manager);
    printf("Max visits test completed.\n");
}

// Test serialization and deserialization
void test_persistence(const char* test_file) {
    printf("\n=== PERSISTENCE TEST ===\n");

    // Create first manager
    printf("Creating first visit manager...\n");
    VisitManager* manager1 = VisitManagerCreate(test_file, 5);
    assert(manager1 != NULL);

    // Add visits
    printf("Adding visits...\n");
    assert(VisitManagerAddVisit(manager1, 4, 401, "https://example.com/persist1", "Persist 1"));
    assert(VisitManagerAddVisit(manager1, 4, 402, "https://example.com/persist2", "Persist 2"));

    // Print visits
    print_user_visits(manager1, 4);

    // Free first manager
    printf("Freeing first visit manager...\n");
    VisitManagerFree(manager1);

    // Create second manager (should load from file)
    printf("Creating second visit manager (should load from file)...\n");
    VisitManager* manager2 = VisitManagerCreate(test_file, 5);
    assert(manager2 != NULL);

    // Print visits (should be the same as before)
    printf("Visits after loading from file:\n");
    print_user_visits(manager2, 4);

    // Add another visit
    printf("Adding another visit...\n");
    assert(VisitManagerAddVisit(manager2, 4, 403, "https://example.com/persist3", "Persist 3"));

    // Print visits again
    print_user_visits(manager2, 4);

    // Clean up
    printf("Freeing second visit manager...\n");
    VisitManagerFree(manager2);
    printf("Persistence test completed.\n");
}

// Test clearing visits for a user
void test_clear(const char* test_file) {
    printf("\n=== CLEAR TEST ===\n");

    // Create manager
    printf("Creating visit manager...\n");
    VisitManager* manager = VisitManagerCreate(test_file, 5);
    assert(manager != NULL);

    // Add visits for user 5
    printf("Adding visits for user 5...\n");
    assert(VisitManagerAddVisit(manager, 5, 501, "https://example.com/clear1", "Clear 1"));
    assert(VisitManagerAddVisit(manager, 5, 502, "https://example.com/clear2", "Clear 2"));
    assert(VisitManagerAddVisit(manager, 5, 503, "https://example.com/clear3", "Clear 3"));

    // Print visits for user 5
    print_user_visits(manager, 5);

    // Clear visits for user 5
    printf("Clearing visits for user 5...\n");
    VisitManagerClear(manager, 5);

    // Print visits for user 5 after clearing
    print_user_visits(manager, 5);

    // Clean up
    printf("Freeing visit manager...\n");
    VisitManagerFree(manager);
    printf("Clear test completed.\n");
}

// Test multiple deletions
void test_multiple_delete(const char* test_file) {
    printf("\n=== MULTIPLE DELETE TEST ===\n");

    // Create manager
    printf("Creating visit manager...\n");
    VisitManager* manager = VisitManagerCreate(test_file, 10);
    assert(manager != NULL);

    // Add visits for user 6
    printf("Adding visits for user 6...\n");
    assert(VisitManagerAddVisit(manager, 6, 601, "https://example.com/multi1", "Multi 1"));
    assert(VisitManagerAddVisit(manager, 6, 602, "https://example.com/multi2", "Multi 2"));
    assert(VisitManagerAddVisit(manager, 6, 603, "https://example.com/multi3", "Multi 3"));
    assert(VisitManagerAddVisit(manager, 6, 604, "https://example.com/multi4", "Multi 4"));
    assert(VisitManagerAddVisit(manager, 6, 605, "https://example.com/multi5", "Multi 5"));

    // Print visits for user 6
    print_user_visits(manager, 6);

    // Delete multiple visits
    printf("Deleting visits 602, 604 for user 6...\n");
    uint32_t to_delete[] = {602, 604};
    assert(VisitManagerDelete(manager, 6, to_delete, 2));

    // Print visits for user 6 after deletion
    print_user_visits(manager, 6);

    // Try to delete non-existent visit
    printf("Trying to delete non-existent visit 999 for user 6...\n");
    uint32_t invalid_delete[] = {999};
    bool result               = VisitManagerDelete(manager, 6, invalid_delete, 1);
    printf("Delete result: %s (expected: false)\n", result ? "true" : "false");

    // Clean up
    printf("Freeing visit manager...\n");
    VisitManagerFree(manager);
    printf("Multiple delete test completed.\n");
}

// Test non-existent user
void test_nonexistent_user(const char* test_file) {
    printf("\n=== NONEXISTENT USER TEST ===\n");

    // Create manager
    printf("Creating visit manager...\n");
    VisitManager* manager = VisitManagerCreate(test_file, 5);
    assert(manager != NULL);

    // Try to get visits for non-existent user
    printf("Getting visits for non-existent user 999...\n");
    size_t count;
    Visit** visits = VisitManagerGetRecentVisits(manager, 999, &count);
    printf("Visit count: %zu (expected: 0)\n", count);
    assert(visits == NULL);
    assert(count == 0);

    // Try to delete visit for non-existent user
    printf("Trying to delete visit for non-existent user 999...\n");
    uint32_t to_delete[] = {101};
    bool result          = VisitManagerDelete(manager, 999, to_delete, 1);
    printf("Delete result: %s (expected: false)\n", result ? "true" : "false");
    assert(!result);

    // Try to clear non-existent user (should not crash)
    printf("Clearing visits for non-existent user 999...\n");
    VisitManagerClear(manager, 999);
    printf("Clear completed without errors.\n");

    // Clean up
    printf("Freeing visit manager...\n");
    VisitManagerFree(manager);
    printf("Nonexistent user test completed.\n");
}

int main() {
    printf("=== VISIT MANAGER TEST PROGRAM ===\n");

    // Run all tests
    test_basic("visit_test.dat");
    test_max_visits("max_visit_test.dat");
    test_persistence("persistence_test.dat");
    test_clear("clear_test.dat");
    test_multiple_delete("multi_delete_test.dat");
    test_nonexistent_user("nonexistent_test.dat");

    printf("\nAll tests completed successfully!\n");
    printf("Run make clean to remove all .dat files\n");

    return 0;
}
