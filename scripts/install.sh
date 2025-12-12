#!/bin/bash

# Configuration
INSTALL_DIR="$HOME/.local/bin"
BINARY_NAME="lumac"
ALIAS_NAME="luma"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SOURCE_DIR/build"

# Colors for logging
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

fail() {
    log_error "$1"
    exit 1
}

# 1. Check dependencies
log_info "Checking dependencies..."
if ! command -v cmake &> /dev/null; then
    fail "cmake is not installed. Please install it first."
fi

if ! command -v make &> /dev/null; then
    fail "make is not installed. Please install it first."
fi

if ! command -v git &> /dev/null; then
    fail "git is not installed. Please install it first."
fi

# 2. Setup Source Directory (Handle Remote Install)
cleanup() {
    if [ -n "$TEMP_DIR" ] && [ -d "$TEMP_DIR" ]; then
        log_info "Cleaning up temporary files..."
        rm -rf "$TEMP_DIR"
    fi
}

if [ ! -f "$SOURCE_DIR/CMakeLists.txt" ]; then
    log_warn "CMakeLists.txt not found in $SOURCE_DIR."
    log_info "Assuming remote installation. Cloning repository..."
    
    TEMP_DIR=$(mktemp -d)
    trap cleanup EXIT
    
    log_info "Created temporary directory: $TEMP_DIR"
    
    if ! git clone https://github.com/puukis/luma.git "$TEMP_DIR/luma"; then
        fail "Failed to clone repository."
    fi
    
    SOURCE_DIR="$TEMP_DIR/luma"
    BUILD_DIR="$SOURCE_DIR/build"
    
    log_success "Repository cloned successfully."
else
    log_info "Found CMakeLists.txt. Running local installation..."
fi

# 3. Build the project
log_info "Building Luma..."

if [ -d "$BUILD_DIR" ]; then
    log_info "Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR" || fail "Could not enter build directory."

log_info "Running CMake..."
if ! cmake ..; then
    fail "CMake configuration failed."
fi

log_info "Compiling..."
if ! make; then
    fail "Compilation failed."
fi

if [ ! -f "$BINARY_NAME" ]; then
    fail "Build completed but binary '$BINARY_NAME' was not found."
fi

log_success "Build successful!"

# 3. Install
log_info "Installing to $INSTALL_DIR..."

mkdir -p "$INSTALL_DIR"

if cp "$BINARY_NAME" "$INSTALL_DIR/"; then
    log_success "Installed $BINARY_NAME to $INSTALL_DIR"
else
    fail "Failed to copy binary to installation directory."
fi

# Create symlink for luma
if ln -sf "$INSTALL_DIR/$BINARY_NAME" "$INSTALL_DIR/$ALIAS_NAME"; then
    log_success "Created alias $ALIAS_NAME pointing to $BINARY_NAME"
else
    log_warn "Failed to create alias $ALIAS_NAME"
fi

# 4. PATH Configuration
log_info "Checking PATH configuration..."

case ":$PATH:" in
    *":$INSTALL_DIR:"*)
        log_success "$INSTALL_DIR is already in your PATH."
        ;;
    *)
        log_warn "$INSTALL_DIR is NOT in your PATH."
        
        SHELL_CONFIG=""
        if [[ "$SHELL" == */zsh ]]; then
            SHELL_CONFIG="$HOME/.zshrc"
        elif [[ "$SHELL" == */bash ]]; then
            SHELL_CONFIG="$HOME/.bashrc"
        fi

        if [ -n "$SHELL_CONFIG" ]; then
            log_info "Adding to $SHELL_CONFIG..."
            echo "" >> "$SHELL_CONFIG"
            echo "# Added by Luma installer" >> "$SHELL_CONFIG"
            echo "export PATH=\"\$PATH:$INSTALL_DIR\"" >> "$SHELL_CONFIG"
            log_success "Added export command to $SHELL_CONFIG"
            log_warn "You may need to restart your terminal or run 'source $SHELL_CONFIG' to use 'lumac' globally."
        else
            log_warn "Could not detect shell configuration file. Please manually add the following to your PATH:"
            echo "export PATH=\"\$PATH:$INSTALL_DIR\""
        fi
        ;;
esac

echo ""
log_success "Installation complete! 🎉"
if  command -v "$INSTALL_DIR/$BINARY_NAME" &> /dev/null; then
    echo "You can test it directly: $INSTALL_DIR/$BINARY_NAME --version" # Assuming --version exists, or just run it
    echo "Or use the alias: $INSTALL_DIR/$ALIAS_NAME <file.lu>"
else
     echo "Installed at: $INSTALL_DIR/$BINARY_NAME"
fi
