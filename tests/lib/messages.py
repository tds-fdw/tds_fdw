class bcolors:
    BOLDRED = '\033[1;31m'
    BOLDGREEN = '\033[1;32m'
    BOLDYELLOW = '\033[1;33m'
    BOLDCYAN = '\033[1;36m'
    BOLDPURPLE = '\033[1;35m'
    RESET = '\033[0m'


def print_error(msg):
    """Print an error message in red"""
    print(bcolors.BOLDRED + "[ERROR] %s" % msg + bcolors.RESET)


def print_warning(msg):
    """Print a warning message in yellow"""
    print(bcolors.BOLDYELLOW + "[WARNING] %s" % msg + bcolors.RESET)


def print_ok(msg):
    """Print an ok message in green"""
    print(bcolors.BOLDGREEN + "[OK] %s" % msg + bcolors.RESET)


def print_info(msg):
    """Print an info message in cyan"""
    print(bcolors.BOLDCYAN + "[INFO] %s" % msg + bcolors.RESET)


def print_report(total, ok, error):
    """Print a test report"""
    if error != 0:
        print_error("=========== TEST REPORT ==============")
        print_error(" OK   : %s" % ok)
        print_error(" ERROR: %s" % error)
        print_error(" Total: %s" % total)
    else:
        print_ok("=========== TEST REPORT ==============")
        print_ok(" OK   : %s" % ok)
        print_ok(" ERROR: %s" % error)
        print_ok(" Total: %s" % total)


def print_usage_error(script, error):
    """Print script's usage and an error

    Keyword arguments:
    script -- A string with the script filename (without path)
    error  -- A string with an error
    """
    print('Usage: %s <arguments>' % script)
    print('')
    print('%s: error: %s' % (script, error))
