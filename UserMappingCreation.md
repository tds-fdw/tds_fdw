# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/UserMappingCreation.md

## Creating a User Mapping

### Options

User mapping parameters accepted:

* *username*  
  
Required: Yes  
  
The username of the account on the foreign server.
				
* *password*  
  
Required: Yes  
  
The password of the account on the foreign server.

### Example

```SQL				
CREATE USER MAPPING FOR postgres
	SERVER mssql_svr 
	OPTIONS (username 'sa', password '');
```