# Issue report

_The following information is very important in order to help us to help you. Omission of the following details cause delays or could receive no attention at all._

## Operating system

_On recent GNU/Linux distributions, you can provide the content of the file `/etc/os-release`_

```
Replace this with the content
```


## Version of tds\_fdw

_From a `psql` session, paste the outputs of running `\dx+`_

_If you built the package from Git sources, also paste the output of running `git log --source -n 1` on your git clone from a console_

```
Replace this with the outputs
```


## Version of PostgreSQL

_From a `psql` session, paste the output of running `SELECT version();`_

```
Replace this with the output
```


## Version of FreeTDS

_How to get it will depend on your Operating System and how you installes FreeTDS_

_From a console:_
* _On RPM based systems: `rpm -qa|grep freetds`_
* _On Deb based systems: `dpkg -l|grep freetds`_
* _If you built your own binaries from source code, then go to the sources, and run: `grep 'AC_INIT' configure.ac`_

```
Replace this with the output
```


## Logs

_Please capture the logs when the error you are reporting is happening, as well as commands with their outputs if you are reporting a problem build or installing_

_For problems using tds_fdw on PostgreSQL how to do it will depend on your system, but if your PostgreSQL is installed on GNU/Linux, you will want to use `tail -f` with the log of the PostgreSQL cluster_

_For MSSQL you will need to use the SQL Server Audit Log_

```
Replace this with the commands and outputs
```

## Sentences, data structures, data

_This will depend on the exact problem you are having and data privacy restrictions_

_However the more data you provide, the more likely we will be able to help_

_As a bare minimum, you should provide_

* _The SQL sentence that is failing_
* _The data structure on the PostgreSQL side and on the MSSQL side_

```
Replace this with the SQL sentences, structus, etc
```
