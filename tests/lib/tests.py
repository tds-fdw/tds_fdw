from glob import glob
from json import load
from lib.messages import print_error, print_info
from os import listdir
from os.path import basename, isfile, realpath
from re import match
from psycopg2.extensions import Diagnostics


def version_to_array(version, dbtype):
    """ Convert a version string to a version array, or return an empty string if
        the original string was empty

    Keyword arguments:
    version -- A string with a dot separated version
    dbtype  -- A string with the database type (postgresql|mssql)
    """
    if version != '':
        # Cleanup version, since Ubuntu decided to add their own
        # versioning starting with PostgreSQL 10.2:
        # '10.2 (Ubuntu 10.2-1.pgdg14.04+1' instead of '10.2'
        version = match('(\d[\.\d]+).*', version).group(1)
        version = version.split('.')
        for i in range(0, len(version)):
            version[i] = int(version[i])
        # To be able to work with MSSQL 2000 and 7.0,
        # see https://sqlserverbuilds.blogspot.ru/
        if dbtype == 'mssql':
            if len(version) == 3:
                version.append(0)
    return(version)


def check_ver(conn, min_ver, max_ver, dbtype):
    """ Check SQL server version against test required max and min versions.

    Keyword arguments:
    conn    -- A db connection (according to Python DB API v2.0)
    min_ver -- A string with the minimum version required
    max_ver -- A string with the maximum version required
    dbtype  -- A string with the database type (postgresql|mssql)
    """
    cursor = conn.cursor()
    if dbtype == 'postgresql':
        sentence = 'SELECT setting FROM pg_settings WHERE name = \'server_version\';'
    elif dbtype == 'mssql':
        sentence = 'SELECT serverproperty(\'ProductVersion\');'
    cursor.execute(sentence)
    server_ver = cursor.fetchone()[0]
    cursor.close()
    min_ver = version_to_array(min_ver, dbtype)
    max_ver = version_to_array(max_ver, dbtype)
    server_ver = version_to_array(server_ver, dbtype)
    if server_ver >= min_ver and (server_ver <= max_ver or max_ver == ''):
        return(True)
    else:
        return(False)

def get_logs_path(conn, dbtype):
    """ Get PostgreSQL logs

    Keyword arguments:
    conn  -- A db connection (according to Python DB API v2.0)
    dbtye -- A string with the database type (postgresql|mssql). mssql will return an empty a array.
    """
    logs = []
    if dbtype == 'mssql':
        return(logs)
    cursor = conn.cursor()
    try:
        cursor.execute("SELECT setting FROM pg_catalog.pg_settings WHERE name = 'data_directory';")
        data_dir = cursor.fetchone()[0]
    except TypeError:
        print_error("The user does not have SUPERUSER access to PostgreSQL.")
        print_error("Cannot access pg_catalog.pg_settings required values, so logs cannot be found")
        return(logs)
    cursor.execute("SELECT setting FROM pg_catalog.pg_settings WHERE name = 'log_directory';")
    log_dir = cursor.fetchone()[0]
    if log_dir[0] != '/':
        log_dir = "%s/%s" % (data_dir, log_dir)
    cursor.execute("SELECT setting FROM pg_catalog.pg_settings WHERE name = 'logging_collector';")
    # No logging collector, add stdout from postmaster (assume stderr is redirected to stdout)
    if cursor.fetchone()[0] == 'off':
        with open("%s/postmaster.pid" % data_dir, "r") as f:
            postmaster_pid = f.readline().rstrip('\n')
        postmaster_log = "/proc/%s/fd/1" % postmaster_pid
        if isfile(postmaster_log):
            logs.append(realpath(postmaster_log))
    # Logging collector enabled
    else:
        # Add stdout from logger (assume stderr is redirected to stdout)
        pids = [pid for pid in listdir('/proc') if pid.isdigit()]
        for pid in pids:
            try:
                cmdline = open('/proc/' + pid + '/cmdline', 'rb').read()
                if 'postgres: logger' in cmdline:
                    logger_log = "/proc/%s/fd/2" % pid
                    if isfile(logger_log):
                        logs.append(realpath(logger_log))
            except IOError: # proc has already terminated
                continue
        # Add all files from log_dir
        for f in listdir(log_dir):
            logs.append(realpath(log_dir + '/' + f))
    return(logs)


def run_tests(path, conn, replaces, dbtype, debugging=False, unattended_debugging=False):
    """Run SQL tests over a connection, returns a dict with results.

    Keyword arguments:
    path     -- String with the path having the SQL files for tests
    conn     -- A db connection (according to Python DB API v2.0)
    replaces -- A dict with replaces to perform at testing code
    dbtype   -- A string with the database type (postgresql|mssql)
    """
    files = sorted(glob(path))
    tests = {'total': 0, 'ok': 0, 'errors': 0}
    for fname in files:
        test_file = open('%s.json' % fname.rsplit('.', 1)[0], 'r')
        test_properties = load(test_file)
        test_desc = test_properties['test_desc']
        test_number = basename(fname).split('_')[0]
        req_ver = test_properties['server']['version']
        if check_ver(conn, req_ver['min'], req_ver['max'], dbtype):
            tests['total'] += 1
            f = open(fname, 'r')
            sentence = f.read()
            for key, elem in replaces.items():
                sentence = sentence.replace(key, elem)
            print_info("%s: Testing %s" % (test_number, test_desc))
            if debugging or unattended_debugging:
                print_info("Query:")
                print(sentence)
            try:
                cursor = conn.cursor()
                cursor.execute(sentence)
                conn.commit()
                cursor.close()
                tests['ok'] += 1
            except Exception as e:
                print_error("Error running %s (%s)" % (test_desc, fname))
                print_error("Query:")
                print(sentence)
                try:
                    print_error(e.pgcode)
                    print_error(e.pgerror)
                    for att in [member for member in dir(Diagnostics) if not member.startswith("__")]:
                        print_error("%s: %s"%(att, getattr(e.diag,att)))
                except:
                    print_error(e)
                conn.rollback()
                tests['errors'] += 1
            f.close()
    return(tests)
