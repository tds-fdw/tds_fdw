from glob import glob
from messages import print_error, print_info


def run_tests(path, conn, replaces):
    """Run SQL tests over a connection, returns a dict with results.

    Keyword arguments:
    path     -- String with the path having the SQL files for tests
    conn     -- A db connection (according to Python DB API v2.0)
    replaces -- A dict with replaces to perform at testing code
    """
    files = sorted(glob(path))
    tests = {'total': 0, 'ok': 0, 'errors': 0}
    for file in files:
        tests['total'] += 1
        f = open(file, 'r')
        sentence = f.read()
        for key, elem in replaces.items():
            sentence = sentence.replace(key, elem)
        print_info("Running %s" % file)
        try:
            cursor = conn.cursor()
            cursor.execute(sentence)
            conn.commit()
            cursor.close()
            tests['ok'] += 1
        except Exception as e:
            print_error("Error running test %s" % file)
            try:
                print_error(e.pgcode)
                print_error(e.pgerror)
            except:
                print_error(e)
            conn.rollback()
            tests['errors'] += 1
        f.close()
    return(tests)
