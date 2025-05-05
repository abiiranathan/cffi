package main

import (
	"cffi"
	"fmt"
	"os"
)

func main() {
	vm, err := cffi.NewVisitManager("rv.dat", 10)
	if err != nil {
		panic(err)
	}
	defer vm.Close()
	defer os.Remove("rv.dat")

	// Create a new visit
	vm.AddVisit(1, 102, "https://google.com", "Google")
	vm.AddVisit(2, 103, "https://github.com", "GitHub")
	vm.AddVisit(3, 104, "https://stackoverflow.com", "Stack Overflow")
	vm.AddVisit(4, 105, "https://example.com", "Example")

	// Get recent visits
	visits, err := vm.GetRecentVisits(2)
	if err != nil {
		panic(err)
	}

	// Print recent visits
	for _, visit := range visits {
		fmt.Println("Visit ID:", visit.VisitID)
		fmt.Println("Timestamp:", visit.Time)
		fmt.Println("URL:", visit.URL)
		fmt.Println("Text:", visit.Text)
		fmt.Println()
	}

}
