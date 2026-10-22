# mini-git
PROJECT TITLE- MINI GIT VERSION CONTROL SYSTEM

Mini-Git is a lightweight version-control system developed as a PBL project using C++. The project aims to understand the basic concepts behind version control systems such as Git by implementing core repository and file-management operations.

The project provides a simple command-line interface and a graphical user interface using Qt. The system is designed to demonstrate how files, commits, branches, and repository information can be managed locally.

Features

- Initialize a local repository
- Add files to the repository
- Create commits with commit messages
- Check repository status
- View commit history
- Create branches
- Switch between branches
- Manage files locally
- Simple graphical interface using Qt
- Local repository storage

Project Objectives

The main objectives of Mini-Git are:

1. To understand the basic working of version-control systems.
2. To implement file and repository management using C++.
3. To understand concepts such as commits and branches.
4. To practice Object-Oriented Programming in C++.
5. To understand file handling and local storage.
6. To develop a simple GUI using the Qt framework.
7. To understand how different components of a software project can work together.

Technologies Used

- C++ – Core programming language
- Qt 6 – Graphical User Interface
- CMake – Build system
- MinGW – C++ compiler
- STL – Data structures and utility functions
- File System – Local repository and file management
- Hashing – Used for identifying stored data

Project Structure

The project is divided mainly into two parts:

Frontend

The frontend is responsible for the graphical interface and user interaction.

It includes:

- Main application window
- Buttons and input fields
- File selection
- Repository controls
- Commit controls
- Status and history display
- Branch controls

The GUI is developed using Qt Widgets.

Backend

The backend contains the core Mini-Git functionality.

It is responsible for:

- Repository initialization
- File handling
- Adding files
- Creating commits
- Maintaining commit information
- Branch management
- Reading and writing repository data
- Local storage operations

The frontend communicates with the backend to perform these operations.

Basic Workflow

A typical Mini-Git workflow is:

Initialize Repository
        ↓
Select/Add Files
        ↓
Create Commit
        ↓
View Status
        ↓
View History
        ↓
Create/Switch Branch

Repository

When a repository is initialized, Mini-Git creates the required local structure for storing project information.

The repository stores information needed to track files and commits.

Adding Files

Users can select files that they want to include in the repository.

The selected files are processed by the backend and stored according to the Mini-Git repository structure.

Commits

A commit represents a saved version of the project at a particular point in time.

Each commit can contain:

- Commit information
- Commit message
- Information about the files included
- Reference to the previous commit where applicable

Users can provide a message describing the changes made.

Branches

Branches allow different versions or lines of development to be maintained separately.

Mini-Git provides basic operations for:

- Creating a branch
- Switching between branches
- Maintaining branch information

Graphical User Interface

The Qt-based GUI provides an easier way to interact with Mini-Git.

The interface contains controls for operations such as:

- Initialize Repository
- Add File
- Commit
- Status
- History
- Create Branch
- Switch Branch

The GUI is being developed separately from the core backend so that the interface and core functionality remain organized.

Future Scope

The project can be extended with additional features such as:

- Improved GUI design
- File and folder browsing
- Better commit history visualization
- Diff between file versions
- Merge functionality
- Conflict detection and resolution
- Remote repository support
- Improved error handling
- Authentication
- More advanced branch management

Learning Outcomes

Through this project, we aim to gain practical understanding of:

- C++ Object-Oriented Programming
- File handling
- Data structures
- Hashing
- Compression
- Version-control concepts
- GUI development using Qt
- CMake-based project building
- Software project structure
- Frontend-backend integration
- Git and GitHub workflow

Team

This project is developed as part of the Project Based Learning (PBL) curriculum.

The team is divided into frontend and backend responsibilities, with collaboration between both parts to integrate the complete Mini-Git system.

Status

The project is currently under development.

The core functionality and graphical interface are being developed and integrated progressively.

---

Mini-Git
A simple educational implementation of version-control concepts using C++ and Qt.
