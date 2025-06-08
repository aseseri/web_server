# Source Code Layout

## The layout of our project can be represented by the following diagram:
```
big-fat-dinosours-w-short-front-legs/
├── CMakeLists.txt 		(builds the project)
├── .gitignore 			(keeps remote repo clean)
├── cmake/ 				(generates coverage report)
├── docker/ 			(container setup— for base, production, coverage, + Cloud Build)
├── include/ 			(header files for source code implementations)
├── src/ 				(src code to run the server, parse config, handle requests, etc.)
└── tests/ 				(contains unit test files, test configs, and integration test script)
	└── test_static/ 	(where temp files are made for static handler tests
```


## CMakeLists.txt Configuration Layout

The top‑level CMakeLists.txt drives all of our build setup. To do so, it:
* Defines the project
* Enforces out‑of‑source builds
* Sets sensible defaults
* Pulls in external dependencies
	* `GoogleTest` for unit testing (via add_subdirectory)
	* `Boost` (static libs: system, log, log_setup, regex)
	* `Python 3` interpreter for integration tests
	* Adds `include/` to the compiler’s header search path
* Declares our core libraries & executable
	* `config_lib` – handles parsing and storing configuration
	* `network_lib` – implements the HTTP server, handlers, MIME types, etc.
	* `webserver `– main executable (src/_main.cc), linked against both libs plus
	* `Boost` and `pthread`
* Configures testing (via `CTest`) for unit tests and our integration test
* Enables code coverage reports
* Provides a reset target
	* `make reset` (or cmake --build . --target reset) wipes out all generated files and cache so you can start fresh

  

# How to Build, Test, and Run the code:
## To build the code:

* Create a /build directory in the root folder of the repo
* Ensure you are in the dev environment, then navigate to the /build directory and run: 
	* `cmake ..`
* After this finished, run: 
	* `make`
* If this completes without error, the build is complete
* If you want to re-build, without deleting the build directory, run the following in sequence:
	* `make reset` (removes old artifacts and runs cmake .. again)
	* `make`

  
## To Test the code (both unit and integration tests):

* Navigate to the /build directory with `cd build`, ensure that the build is complete:
* Run:
	* `make test` OR `ctest`

## To run the code, and handle static/echo requests:

1. Create a config that you want to be used for this server instance in the /tests directory (for example, see: `my_config` or `temp_config`)
2. Configure the config with the server port and handler paths you desire for echo and static handling
3. Create relevant files for the static handler at the indicated path
4. Navigate to the /build directory, ensure that the build is complete

## To start the server, run:
1. `/bin/webserver ../tests/my_config`
2. If you want to serve a static file request, run the following in another terminal:
`curl -i http://localhost:8080/static/{$RELATIVE-PATH-TO-STATIC-FILE}`
4. If you want to serve an echo request, run either of the following in another terminal:
`curl -i http://localhost:${PORT}/echo`
	```
	curl -i \
	-X POST \
	-H "User-Agent: TestAgent/1.0" \
	-H "X-Custom-Header: test-value" \
	-d "hello_echo" 
	\http://localhost:${PORT}/echo/my/path
	```
  
  

# How to add another Handler:
In our code, a base IRequestHandler class creates the virtual functions CanHandle and HandleRequest. These are overridden by derived classes such as EchoHandler, StaticHandler, and NotFoundHandler.

  

## To add a new handler, one should:

1. Create a new `.h` and `.cc` file for their handler, having it extend the `IRequestHandler class` and override the two virtual IRequestHandler functions, `CanHandle` and `HandleRequest` at minimum.
2. After, they should also add their handler to the mapping member `handlers_` in server_runner.cc’s setup_server function so that the handler’s methods are callable.
3. Then, they should register the new .cc file in CMakeLists.txt’s `network_lib` library by adding the .cc file to network_lib’s `add_library() `call.
4. Additionally, one could create unit tests via a `<handler_name>_test.cc` file in the `tests/` directory, and register the test by adding it as an executable via `add_executable()`, linking it with the required libraries via `target_link_libraries()`, and making it discoverable by gtest via `gtest_discover_tests()`.

  

## Example of a Handler Implementation (EchoHandler)

### The base class is setup as the following: request_handler.h
```cpp
struct IRequestHandler {
virtual ~IRequestHandler() = default;

// “Can I serve this request?”
virtual bool CanHandle(const HttpRequest &req) const = 0;

// Fill in resp (status‐line, headers, body)
virtual void HandleRequest(const HttpRequest &req, HttpResponse &resp) = 0;
};
using IRequestHandlerPtr = std::shared_ptr<IRequestHandler>;
```
  
### EchoHandler extends the base class via the following: echo_handler.h

```cpp
class EchoHandler : public IRequestHandler {
public:

// returns true if the request is an echo request
bool CanHandle(const HttpRequest &req) const override;

// fills in resp with the request details
// including the raw request string
void HandleRequest(const HttpRequest &req, HttpResponse &resp) override;
};
```
  

### EchoHandler overrides the virtual functions with the following implementation: echo_handler.cc (omitting logs)
```cpp
// returns true if the request is an echo request
bool EchoHandler::CanHandle(const HttpRequest &req) const {
	static constexpr char const* PREFIX = "/echo";
	// exactly “/echo” or any path that starts with “/echo/”
	return req.path == PREFIX
	|| req.path.rfind(std::string(PREFIX) + "/", 0) == 0;
}

// fills in resp with the request details
// including the raw request string
void EchoHandler::HandleRequest(const HttpRequest &req, HttpResponse &resp) {

resp.version = "HTTP/1.1";
resp.status_code = 200;
resp.reason = "OK";
resp.headers["Content-Type"] = "text/plain";
resp.body = req.raw; // echo entire raw request
}
```
  
### Then, in server_runner.cc’s setup_server function, an instance of EchoHandler is added to the handler mappings:
```cpp
{...}
// Build handler list from every top‐level 'handler { ... }' block
handlers_.clear();
auto handlers_conf = config.GetHandlerConfigs();
for (auto &hc : handlers_conf) {
	if (hc.type == "echo") 
	handlers_.push_back(std::static_pointer_cast<IRequestHandler>
		(std::make_shared<EchoHandler>()));
	else if (hc.type == "static"){
		std::string  root = hc.args.count("root") ? hc.args["root"] : "";
		handlers_.push_back(std::static_pointer_cast<IRequestHandler>(
			std::make_shared<StaticHandler>(hc.path, hc.root)));
	}
}
handlers_.push_back(std::static_pointer_cast<IRequestHandler>(
	std::make_shared<NotFoundHandler>()));
```
  

### Then, when the session is established, upon receiving a request, EchoHandler’s CanHandle and HandleRequest functions are called if the request is an echo request in session’s handle_read function:
```cpp
{...}
HttpResponse resp;
for (auto &h : handlers_) {
	if (h->CanHandle(req)) {
		h->HandleRequest(req, resp);
	}
	{...}
}
```

# Handler Header and API Documentation

## http_request.h

Parses TCP data into an HttpRequest object, extracting method, path, headers, and body.

### static HttpRequest Parse(const std::string &raw_req);
	Parses the raw HTTP request into an HttpRequest Object
	Stores the request into method, path, version, headers, 
	body, and raw request data

## http_response.h

Builds HttpResponse objects, letting handlers set status codes, headers, and body content which then get flattened into bytes to send back over the network.

### std::string ToString() const;
	Returns the response as a string

### static HttpResponse Stock404();
	Returns a 404 response

## mime_types.h
Contains a lookup table mapping file extensions (e.g. .html, .png) to their correct MIME strings—used for serving static files so clients know how to interpret the payload.

### static const std::unordered_map<std::string,  std::string> &ExtensionMap();
	Builds a map of common MIME type extensions

### static std::string LookupByExtension(const std::string &path);
	Extracts an extension from the filename/path, and 
	determines the MIME type

## request_handler.h

A pure virtual abstract class that documents common functions implemented/overridden by all derived handlers (echo, static, 404)

## echo_handler.h

Provides a simple HTTP handler that echoes the requests it gets.

### bool CanHandle(const HttpRequest &req) const override;

	Determines if the HTTP Request is an echo request based on the path

### void HandleRequest(const HttpRequest &req, HttpResponse &resp) override;

	Builds response with the request details to echo it back
	Includes the raw request in the response

## static_handler.h

Provides the StaticHandler, which maps URL paths to files on disk under your webroot: it checks for file existence, reads file contents into the response body, and uses the MIME lookup (mime_types.cc) to set the correct Content-Type header when serving assets.

### StaticHandler(std::string uri_prefix, std::string doc_root): prefix_(std::move(uri_prefix)), root_(std::move(doc_root)) {}
	 Takes a prefix and root and saves them as member variables

### bool CanHandle(const HttpRequest &req) const override;
	Determines if the request is to a static file based on the path

### void HandleRequest(const HttpRequest &req, HttpResponse &resp) override;
	Builds request by serving the file at the path, filling in the HTTP response information along with the body of the file information

## not_found_handler.h
Implements the fallback “404 Not Found” handler: when no other route matches, this returns a 404 status and a simple error page.

### bool CanHandle(const HttpRequest& req) const override;

	Always returns true to handle all requests as the fallback

### void HandleRequest(const HttpRequest& req, HttpResponse& resp) override;

	Returns a standard 404 response

## server_runner.h

Orchestrates the main server loop—binding to your chosen port, accepting connections, handing off each parsed HttpRequest to the right handler, and sending back the resulting HttpResponse.

### Contains our handler injector as a member variable:

	static std::vector<IRequestHandlerPtr> handlers_;

### bool ServerRunner::setup_server(const std::string &config_path, int &port)

```
Sets up the server by parsing the config file, retrieving 
& validating the port number, and builds the handler mapping, 
with the 404 handler being the last one, as a fallback
```

## server.h

Defines the Server class, which glues together configuration, handler registration, and the server runner: allowing ServerRunner to begin accepting connections.

## session.h

Implements the Session abstraction for a single client connection—responsible for reading raw socket data, invoking the request parser, dispatching the parsed HttpRequest to the correct handler, and writing back the serialized HttpResponse, all while managing connection lifetimes (keep‑alive, timeouts).

### void session::handle_read(const boost::system::error_code &ec, std::size_t bytes)
```
Determines which handler in the mapping is able to serve 
the incoming HTTP request and calls the corresponding handler
```

## CRUD API Endpoints

Our `CRUDHandler` listens under an `/api` prefix.  You can perform standard CRUD (Create, Read, Update, Delete) operations on any “entity” (e.g. Shoes, Users, Products) by hitting these URLs:


## CRUD API Endpoints

We expose the following endpoints under `/api/{entity}`:

| Action  | Method   | Path                   | Description                     |
|---------|----------|------------------------|---------------------------------|
| Create  | POST     | /api/{entity}          | Create a new resource           |
| List    | GET      | /api/{entity}          | Retrieve all resources          |
| Read    | GET      | /api/{entity}/{id}     | Retrieve one resource by ID     |
| Update  | PUT      | /api/{entity}/{id}     | Update an existing resource     |
| Delete  | DELETE   | /api/{entity}/{id}     | Delete an existing resource     |

All requests and responses use `application/json`.

### Request & Response Format

Request Headers  
```
Content-Type: application/json
Accept: application/json
```

Request Body  
```json
{
	// entity-specific fields
}
```

Response Body  
- **Create/Read/Update**: full JSON of the resource, including `id`.
- **Delete**: JSON message confirming deletion.

---

## Full Example: Shoes

#### 1. Create a Shoe  
Request  
```
curl -i -X POST http://localhost:8080/api/shoes \
	-H "Content-Type: application/json" \
	-d '{"name":"AirMax","size":9,"color":"red"}'
```
Response  
```http
HTTP/1.1 201 Created
Content-Type: application/json

{
	"id": 1,
	"name": "AirMax",
	"size": 9,
	"color": "red"
}
```

#### 2. List All Shoes  
Request  
```
curl -i http://localhost:8080/api/shoes
```
Response  
```http
HTTP/1.1 200 OK
Content-Type: application/json

[
	{
		"id": 1,
		"name": "AirMax",
		"size": 9,
		"color": "red"
	}
]
```

#### 3. Get a Shoe by ID  
Request  
```
curl -i http://localhost:8080/api/shoes/1
```
Response  
```http
HTTP/1.1 200 OK
Content-Type: application/json

{
	"id": 1,
	"name": "AirMax",
	"size": 9,
	"color": "red"
}
```

#### 4. Update a Shoe  
Request  
```
curl -i -X PUT http://localhost:8080/api/shoes/1 \
	-H "Content-Type: application/json" \
	-d '{"size":10}'
```
Response  
```http
HTTP/1.1 200 OK
Content-Type: application/json

{
	"id": 1,
	"name": "AirMax",
	"size": 10,
	"color": "red"
}
```

#### 5. Delete a Shoe  
Request  
```
curl -i -X DELETE http://localhost:8080/api/shoes/1
```
Response  
```http
HTTP/1.1 200 OK
Content-Type: application/json

{
	"message": "Shoe with id 1 deleted successfully"
}
```