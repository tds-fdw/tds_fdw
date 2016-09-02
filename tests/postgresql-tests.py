#!/usr/bin/env python
from lib.messages import print_error, print_info, print_ok, print_report
from lib.messages import print_usage_error, print_warning
from lib.tests import run_tests
from optparse import OptionParser
from os import path
try:
    from psycopg2 import connect
except:
    print_error(
        "psycopg2 library not available, please install it before usage!")
    exit(1)


def parse_options():
    """Parse and validate options. Returns a dict with all the options."""
    usage = "%prog <arguments>"
    description = ('Run PostgreSQL tests from sql files')
    parser = OptionParser(usage=usage, description=description)
    parser.add_option('--postgres_server', action='store',
                      help='PostgreSQL server')
    parser.add_option('--postgres_port', action='store',
                      help='PostgreSQL  TCP port')
    parser.add_option('--postgres_database', action='store',
                      help='Database name')
    parser.add_option('--postgres_schema', action='store',
                      help='Schema to use (and create if needed)')
    parser.add_option('--postgres_username', action='store',
                      help='Username to connect')
    parser.add_option('--postgres_password', action='store',
                      help='Passord to connect')
    parser.add_option('--mssql_server', action='store',
                      help='MSSQL/Azure server')
    parser.add_option('--mssql_port', action='store',
                      help='MSSQL/Azure  TCP port')
    parser.add_option('--mssql_database', action='store',
                      help='Database name')
    parser.add_option('--mssql_schema', action='store',
                      help='Schema to use (and create if needed)')
    parser.add_option('--mssql_username', action='store',
                      help='Username to connect')
    parser.add_option('--mssql_password', action='store',
                      help='Passord to connect')
    parser.add_option('--azure', action="store_true", default=False,
                      help='If present, will connect as Azure, otherwise as '
                           'standard MSSQL')
    (options, args) = parser.parse_args()
    # Check for test parameters
    if (options.postgres_server is None or
        options.postgres_port is None or
        options.postgres_database is None or
        options.postgres_schema is None or
        options.postgres_username is None or
        options.postgres_password is None or
        options.mssql_server is None or
        options.mssql_port is None or
        options.mssql_database is None or
        options.mssql_schema is None or
        options.mssql_username is None or
            options.mssql_password is None):
        print_error("Insufficient parameters, check help (-h)")
        exit(4)
    else:
        if options.azure is True:
            options.mssql_username = '%s@%s' % (options.mssql_username,
                                                options.mssql_server.split('.')[0])
        return(options)


def main():
    try:
        args = parse_options()
    except Exception as e:
        print_usage_error(path.basename(__file__), e)
        exit(2)
    try:
        conn = connect(host=args.postgres_server, user=args.postgres_username,
                       password=args.postgres_password,
                       database=args.postgres_database,
                       port=args.postgres_port)
        replaces = {'@PSCHEMANAME': args.postgres_schema,
                    '@PUSER': args.postgres_username,
                    '@MSERVER': args.mssql_server,
                    '@MPORT': args.mssql_port,
                    '@MUSER': args.mssql_username,
                    '@MPASSWORD': args.mssql_password,
                    '@MDATABASE': args.mssql_database,
                    '@MSCHEMANAME': args.mssql_schema}
        tests = run_tests('tests/postgresql/*.sql', conn, replaces, 'postgresql')
        print_report(tests['total'], tests['ok'], tests['errors'])
        if tests['errors'] != 0:
            exit(5)
        else:
            exit(0)
    except Exception as e:
        print_error(e)
        exit(3)

if __name__ == "__main__":
    main()
