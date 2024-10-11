# File Search Tool
This program is a command-line tool that searches for specific files in a given directory. It supports recursive search and case-insensitive file matching.

## How it works
The program scans a specified directory and searches for the provided filenames. It can search for multiple files simultaneously by creating child processes (via `fork`) to parallelize the work. The results are sent to the main process using pipes and then output.

## Features
- **Recursive Search**: Recursively searches directories for files.
- **Case-Insensitive Search**: Optionally, the search can be case-insensitive.
- **Parallel Processing**: A separate process is started for each file being searched.
- **Pipes**: Pipes are used for communication between parent and child processes.