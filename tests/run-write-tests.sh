#!/bin/bash
# Quick test script for write operations testing
# This script provides examples of running the write operation tests

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration - EDIT THESE VALUES
MSSQL_SERVER="${MSSQL_SERVER:-localhost}"
MSSQL_PORT="${MSSQL_PORT:-1433}"
MSSQL_DATABASE="${MSSQL_DATABASE:-testdb}"
MSSQL_SCHEMA="${MSSQL_SCHEMA:-tds_fdw_tests}"
MSSQL_USERNAME="${MSSQL_USERNAME:-sa}"
MSSQL_PASSWORD="${MSSQL_PASSWORD}"

PG_SERVER="${PG_SERVER:-localhost}"
PG_PORT="${PG_PORT:-5432}"
PG_DATABASE="${PG_DATABASE:-postgres}"
PG_SCHEMA="${PG_SCHEMA:-tds_fdw_pg_tests}"
PG_USERNAME="${PG_USERNAME:-postgres}"
PG_PASSWORD="${PG_PASSWORD}"

TDS_VERSION="${TDS_VERSION:-7.1}"

# Function to print colored messages
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if passwords are set
if [ -z "$MSSQL_PASSWORD" ]; then
    print_error "MSSQL_PASSWORD environment variable not set"
    exit 1
fi

if [ -z "$PG_PASSWORD" ]; then
    print_error "PG_PASSWORD environment variable not set"
    exit 1
fi

# Check if scripts exist
if [ ! -f "mssql-tests.py" ]; then
    print_error "mssql-tests.py not found. Run this script from tests/ directory"
    exit 1
fi

if [ ! -f "postgresql-tests.py" ]; then
    print_error "postgresql-tests.py not found. Run this script from tests/ directory"
    exit 1
fi

# Print configuration
print_info "Test Configuration:"
echo "  MSSQL Server: $MSSQL_SERVER:$MSSQL_PORT"
echo "  MSSQL Database: $MSSQL_DATABASE"
echo "  MSSQL Schema: $MSSQL_SCHEMA"
echo "  PostgreSQL Server: $PG_SERVER:$PG_PORT"
echo "  PostgreSQL Database: $PG_DATABASE"
echo "  PostgreSQL Schema: $PG_SCHEMA"
echo "  TDS Version: $TDS_VERSION"
echo ""

# Step 1: Run MSSQL setup tests
print_info "Step 1: Running MSSQL setup tests..."
./mssql-tests.py \
    --server "$MSSQL_SERVER" \
    --port "$MSSQL_PORT" \
    --database "$MSSQL_DATABASE" \
    --schema "$MSSQL_SCHEMA" \
    --username "$MSSQL_USERNAME" \
    --password "$MSSQL_PASSWORD" \
    --tds_version "$TDS_VERSION"

if [ $? -eq 0 ]; then
    print_info "MSSQL setup tests completed successfully"
else
    print_error "MSSQL setup tests failed"
    exit 1
fi

echo ""

# Step 2: Run PostgreSQL FDW tests
print_info "Step 2: Running PostgreSQL FDW write operation tests..."
./postgresql-tests.py \
    --postgres_server "$PG_SERVER" \
    --postgres_port "$PG_PORT" \
    --postgres_database "$PG_DATABASE" \
    --postgres_schema "$PG_SCHEMA" \
    --postgres_username "$PG_USERNAME" \
    --postgres_password "$PG_PASSWORD" \
    --mssql_server "$MSSQL_SERVER" \
    --mssql_port "$MSSQL_PORT" \
    --mssql_database "$MSSQL_DATABASE" \
    --mssql_schema "$MSSQL_SCHEMA" \
    --mssql_username "$MSSQL_USERNAME" \
    --mssql_password "$MSSQL_PASSWORD" \
    --tds_version "$TDS_VERSION"

if [ $? -eq 0 ]; then
    print_info "PostgreSQL FDW tests completed successfully"
    print_info "All write operation tests PASSED! ✓"
else
    print_error "PostgreSQL FDW tests failed"
    print_warning "Check the output above for details"
    exit 1
fi

echo ""
print_info "Test Summary:"
echo "  - MSSQL tables created: ✓"
echo "  - Foreign tables created: ✓"
echo "  - INSERT operations tested: ✓"
echo "  - UPDATE operations tested: ✓"
echo "  - DELETE operations tested: ✓"
echo "  - NULL handling tested: ✓"
echo "  - Data types tested: ✓"
echo "  - Error handling tested: ✓"
echo "  - Complete workflow tested: ✓"
