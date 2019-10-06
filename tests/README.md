# Testing scripts

Testing should follow that workflow :

  * First build a MSSQL Server
    1. Create the server (local, container, VM or azure)
    2. Create a testing database with a proper user and access privileges
    3. Run `mssql-tests.py` against that server.
  * Next, build a PostgreSQL Server
    1. Create the server (on your machine, with docker o rVM)
    2. Compile and install tds_fdw extension
    3. Create a testing database and schema with proper user and access privilege
    4. On that database you'll have first to install tds_fdw: `CREATE EXTENSION tds_fdw;`
    5. You can run `postgresql_test.py`

# Debugging

It may be interesting to build a full setup for debugging purpose and use tests to check if anythong regresses.
For this, you can use `--debugging` parameter at `postgresql-tests.py` launch time.

The test program will stop just after connection creation and give you the backend PID used for testing. This will allow you to connect gdb in another shell session (`gdb --pid=<PID>`). Once connected with gdb, just put breakpoints where you need and run `cont`. Then you can press any key in the test shell script to start testing.

Also, in case of test failure, `--debugging` will allow to define quite precisely where the script crashed using psycopg2 `Diagnostics` class information and will give the corresponding SQL injected in PostgreSQL.