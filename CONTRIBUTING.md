# Contributing to CogZero

Thank you for your interest in contributing to CogZero! This document provides guidelines and instructions for contributing to the project.

## 🎯 Project Vision

CogZero is a high-performance C++ implementation of Agent-Zero optimized for integration with OpenCog's cognitive architecture. Our goal is to create a well-orchestrated and modular catalog of powerful cognitive tools, skills, abilities, and knowledge enhancements.

## 📋 Table of Contents

- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Code Standards](#code-standards)
- [Testing Guidelines](#testing-guidelines)
- [Documentation](#documentation)
- [Pull Request Process](#pull-request-process)
- [Issue Guidelines](#issue-guidelines)
- [Community Guidelines](#community-guidelines)

## 🚀 Getting Started

### Prerequisites

1. **Read the Documentation**
   - [README.md](README.md) - Project overview
   - [AGENT-ZERO-GENESIS.md](AGENT-ZERO-GENESIS.md) - Complete roadmap
   - [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - System architecture
   - [docs/DEVELOPER_GUIDE.md](docs/DEVELOPER_GUIDE.md) - Development guide

2. **Set Up Development Environment**
   ```bash
   # Clone the repository
   git clone https://github.com/o9nn/cogzero.git
   cd cogzero
   
   # Install dependencies (see README.md)
   # Build the project
   mkdir build && cd build
   cmake -DCMAKE_BUILD_TYPE=Debug ..
   make -j$(nproc)
   
   # Run tests
   make test
   ```

3. **Find an Issue**
   - Browse [open issues](https://github.com/o9nn/cogzero/issues)
   - Look for issues labeled `good-first-issue` or `help-wanted`
   - Check the [roadmap](AGENT-ZERO-GENESIS.md) for planned features

## 🔄 Development Workflow

### 1. Fork and Clone

```bash
# Fork the repository on GitHub
# Clone your fork
git clone https://github.com/YOUR_USERNAME/cogzero.git
cd cogzero

# Add upstream remote
git remote add upstream https://github.com/o9nn/cogzero.git
```

### 2. Create a Branch

```bash
# Update your fork
git checkout main
git pull upstream main

# Create a feature branch
git checkout -b feature/your-feature-name
# or
git checkout -b fix/issue-number-description
```

### 3. Make Changes

- Write clean, well-documented code
- Follow the [Code Standards](docs/CODE_STANDARDS.md)
- Add tests for new functionality
- Update documentation as needed

### 4. Test Your Changes

```bash
# Build with tests
cd build
cmake ..
make -j$(nproc)

# Run all tests
make test

# Run specific test suites
ctest -R unit_tests
ctest -R integration_tests

# Check for memory leaks (optional)
valgrind --leak-check=full ./your_test_binary
```

### 5. Commit Your Changes

```bash
# Stage your changes
git add .

# Commit with a descriptive message
git commit -m "Add feature: Brief description

Detailed explanation of what was changed and why.
Fixes #issue_number"
```

**Commit Message Format:**
```
<type>: <subject>

<body>

<footer>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `chore`: Maintenance tasks

### 6. Push and Create Pull Request

```bash
# Push to your fork
git push origin feature/your-feature-name

# Create a pull request on GitHub
```

## 📝 Code Standards

### C++ Guidelines

1. **C++17 Standard**
   - Use modern C++ features
   - Prefer `auto` where type is obvious
   - Use smart pointers (`std::unique_ptr`, `std::shared_ptr`)

2. **Naming Conventions**
   ```cpp
   // Classes: PascalCase
   class AgentZeroCore { };
   
   // Functions/Methods: camelCase
   void processPerception() { }
   
   // Variables: camelCase
   int perceptionCount = 0;
   
   // Constants: UPPER_SNAKE_CASE
   const int MAX_ITERATIONS = 1000;
   
   // Namespaces: lowercase
   namespace opencog { namespace agentzero { } }
   ```

3. **Code Organization**
   - Header files in `include/opencog/agentzero/`
   - Implementation files in `src/`
   - One class per file (generally)
   - Include guards or `#pragma once`

4. **Documentation**
   ```cpp
   /**
    * Brief description of the class/function
    *
    * Detailed description explaining the purpose,
    * behavior, and any important notes.
    *
    * @param param1 Description of parameter
    * @return Description of return value
    * @throws ExceptionType When this exception is thrown
    */
   ```

5. **Error Handling**
   - Use exceptions for exceptional conditions
   - Use return codes for expected failures
   - Document all exceptions that can be thrown

See [docs/CODE_STANDARDS.md](docs/CODE_STANDARDS.md) for complete guidelines.

## 🧪 Testing Guidelines

### Test Requirements

1. **Unit Tests**
   - Test individual classes and functions
   - Use mocks for dependencies
   - Aim for >80% code coverage

2. **Integration Tests**
   - Test module interactions
   - Test OpenCog integration
   - Test real-world scenarios

3. **Performance Tests**
   - Benchmark critical paths
   - Test scalability
   - Monitor memory usage

### Writing Tests

```cpp
#include <cxxtest/TestSuite.h>
#include <opencog/agentzero/YourClass.h>

class YourClassTest : public CxxTest::TestSuite
{
public:
    void setUp() {
        // Setup test fixtures
    }
    
    void tearDown() {
        // Cleanup
    }
    
    void testBasicFunctionality() {
        // Arrange
        YourClass obj;
        
        // Act
        auto result = obj.doSomething();
        
        // Assert
        TS_ASSERT_EQUALS(result, expectedValue);
    }
};
```

See [docs/TESTING_GUIDE.md](docs/TESTING_GUIDE.md) for detailed testing guidelines.

## 📚 Documentation

### Required Documentation

1. **Code Documentation**
   - Document all public APIs
   - Explain complex algorithms
   - Add usage examples

2. **README Updates**
   - Update module READMEs for new features
   - Add examples for new functionality
   - Update dependency lists

3. **Architecture Documentation**
   - Document design decisions
   - Update architecture diagrams
   - Explain integration patterns

### Documentation Format

- Use Markdown for all documentation
- Follow existing documentation structure
- Include code examples where appropriate
- Keep documentation up-to-date with code changes

## 🔀 Pull Request Process

### Before Submitting

- [ ] Code follows the style guidelines
- [ ] All tests pass
- [ ] New tests added for new functionality
- [ ] Documentation updated
- [ ] Commit messages are clear and descriptive
- [ ] Branch is up-to-date with main

### PR Description Template

```markdown
## Description
Brief description of the changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Related Issues
Fixes #issue_number

## Testing
Describe the tests you ran and how to reproduce

## Checklist
- [ ] Code follows style guidelines
- [ ] Self-review completed
- [ ] Comments added for complex code
- [ ] Documentation updated
- [ ] Tests added/updated
- [ ] All tests pass
```

### Review Process

1. **Automated Checks**
   - CI/CD pipeline runs automatically
   - All tests must pass
   - Code coverage must not decrease

2. **Code Review**
   - At least one maintainer review required
   - Address all review comments
   - Request re-review after changes

3. **Merge**
   - Squash and merge for feature branches
   - Merge commit for important features
   - Delete branch after merge

## 🐛 Issue Guidelines

### Reporting Bugs

Use the bug report template:

```markdown
## Bug Description
Clear description of the bug

## Steps to Reproduce
1. Step 1
2. Step 2
3. ...

## Expected Behavior
What should happen

## Actual Behavior
What actually happens

## Environment
- OS: [e.g., Ubuntu 22.04]
- Compiler: [e.g., GCC 11.3]
- OpenCog version: [e.g., 5.0.3]
- CogZero version: [e.g., 0.1.0]

## Additional Context
Any other relevant information
```

### Feature Requests

Use the feature request template:

```markdown
## Feature Description
Clear description of the proposed feature

## Motivation
Why is this feature needed?

## Proposed Solution
How should this be implemented?

## Alternatives Considered
Other approaches you've thought about

## Additional Context
Any other relevant information
```

## 🤝 Community Guidelines

### Code of Conduct

- Be respectful and inclusive
- Welcome newcomers
- Provide constructive feedback
- Focus on the code, not the person
- Assume good intentions

### Communication

- **GitHub Issues**: Bug reports and feature requests
- **Pull Requests**: Code contributions and discussions
- **Discussions**: General questions and ideas

### Getting Help

- Check [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)
- Search existing issues
- Ask in GitHub Discussions
- Join OpenCog community forums

## 📊 Development Phases

The project follows a phased development approach. See [AGENT-ZERO-GENESIS.md](AGENT-ZERO-GENESIS.md) for details.

| Phase | Focus | Status |
|-------|-------|--------|
| Phase 1 | Foundation Layer | 🚧 In Progress |
| Phase 2 | Perception & Action | 📅 Planned |
| Phase 3 | Knowledge & Reasoning | 📅 Planned |
| Phase 4 | Planning & Goals | 📅 Planned |
| Phase 5 | Learning & Adaptation | 📅 Planned |
| Phase 6 | Communication & NLP | 📅 Planned |
| Phase 7 | Memory & Context | 📅 Planned |
| Phase 8 | Tool Integration | 📅 Planned |
| Phase 9 | Integration & Testing | 📅 Planned |
| Phase 10 | Advanced Features | 📅 Planned |

## 🏷️ Issue Labels

- `good-first-issue`: Good for newcomers
- `help-wanted`: Extra attention needed
- `bug`: Something isn't working
- `enhancement`: New feature or request
- `documentation`: Documentation improvements
- `testing`: Testing related
- `phase-N`: Development phase (1-10)
- `priority-{high|medium|low}`: Priority level

## 📄 License

By contributing to CogZero, you agree that your contributions will be licensed under the AGPL-3.0-or-later license.

## 🙏 Recognition

Contributors will be recognized in:
- GitHub contributors page
- Release notes
- Project documentation

---

Thank you for contributing to CogZero! Your efforts help advance cognitive architecture research and development.

For questions, please open an issue or discussion on GitHub.
