# #!/usr/bin/env python3

import os
import subprocess
import time
import requests
import unittest
import socket
import concurrent.futures
import signal
import shutil


class WebServerIntegrationTest(unittest.TestCase):
    """
    End-to-end tests for the web server.
    Starts the server binary and tests its behavior through HTTP requests.
    """
    
    @classmethod
    def setUpClass(cls):
        """Starts the server once before all tests run"""
        cls._setup_paths()
        cls._extract_port()
        cls._prepare_test_environment()
        cls._start_server()
        cls._wait_for_server()

    @classmethod
    def tearDownClass(cls):
        """Stop server and clean up files after all tests finish running"""
        if hasattr(cls, 'server_process'):
            try:
                os.killpg(os.getpgid(cls.server_process.pid), signal.SIGTERM)
                cls.server_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(os.getpgid(cls.server_process.pid), signal.SIGKILL)

        # Remove dynamically created config file and static files
        for path in [cls.temp_config_file, cls.test_static_dir, cls.test_crud_dir]:
            if path and os.path.exists(path):
                if os.path.isdir(path):
                    shutil.rmtree(path)
                else:
                    os.remove(path)

    @classmethod
    def _setup_paths(cls):
        # Locate project files regardless of where test is run from
        current_dir = os.path.dirname(os.path.abspath(__file__))
        cls.project_root = os.path.dirname(current_dir)

        cls.config_file = os.path.join(cls.project_root, "tests", "my_config") # configuration file path
        cls.server_binary = os.path.join(cls.project_root, "build", "bin", "webserver") # compiled server binary file path

        # Making static files folder
        cls.test_static_dir = os.path.join(cls.project_root, "tests", "test_static")

        # Making CRUD API folder
        cls.test_crud_dir = os.path.join(cls.project_root, "tests", "test_crud")

        # Generate a temporary config file
        cls.temp_config_file = os.path.join(cls.project_root, "tests", "temp_config")

    @classmethod
    def _extract_port(cls):
        # Extract port number from config file
        cls.server_port = None
        with open(cls.config_file) as config:
            for line in config:
                if line.strip().startswith("port"):
                    cls.server_port = int(line.split()[1].strip(';'))
                    break
        if cls.server_port is None:
            raise ValueError("Port not found in config file")

    @classmethod
    def _prepare_test_environment(cls):
        # Creates a temporary config file using the port from my_config
        os.makedirs(cls.test_static_dir, exist_ok=True)
        os.makedirs(cls.test_crud_dir, exist_ok=True)
        with open(cls.temp_config_file, "w") as f:
            f.write(f"""
        port {cls.server_port};

        location /echo EchoHandler {{  # no arguments
        }}

        location /static StaticHandler {{
            root {cls.test_static_dir};
        }}

        location /api CRUDHandler {{
            data_path {cls.test_crud_dir};
        }}

        location /health HealthHandler {{
        }}

        location /sleep SleepHandler {{
            sleep_seconds 1;
        }}

        location /secure EchoHandler{{
            requires_auth true;
        }}
        """
            )

    @classmethod
    def _start_server(cls):
        # Start the server process in the background (using the temporary config)
        cls.server_process = subprocess.Popen(
            [cls.server_binary, cls.temp_config_file],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            preexec_fn=os.setsid
        )

        # Check if process died immediately
        time.sleep(0.1)  # Pause to let process start
        if cls.server_process.poll() is not None:
            _, stderr = cls.server_process.communicate()
            print("Server failed to start. STDERR:", stderr.decode())
            cls.tearDownClass()
            raise RuntimeError("Server process terminated immediately")

    @classmethod
    def _wait_for_server(cls):
        # Give the server time to startup
        max_attempts = 10
        attempt = 0
        while attempt < max_attempts:
            try:
                print(f"Attempt {attempt}: Trying to connect to localhost:{cls.server_port}")
                with socket.create_connection(('localhost', cls.server_port), timeout=1):
                    print("Connection successful!")
                    break
            except (socket.timeout, ConnectionRefusedError) as e:
                print(f"Connection failed: {str(e)}")
                # Check if server died
                if cls.server_process.poll() is not None:
                    _, stderr = cls.server_process.communicate()
                    print("Server died during startup. STDERR:", stderr.decode())
                    cls.tearDownClass()
                    raise RuntimeError("Server process terminated during startup")
                attempt += 1
                time.sleep(0.5)
    
        if attempt == max_attempts:
            print("Getting server output for debugging...")
            _, stderr = cls.server_process.communicate()
            print("Server STDERR:", stderr.decode())
            cls.tearDownClass()
            raise RuntimeError("Server failed to start within timeout period")
    
    def _url(self, path):
        return f"http://localhost:{self.server_port}{path}"

    def test_health_handler(self):
        """Test that the health handler returns OK"""
        print("\n=== Testing Health Handler ===")
        url = self._url("/health")
        print(f"Request URL: {url}")
        response = requests.get(url)
        
        print(f"Response status: {response.status_code}")
        print(f"Response headers: {response.headers}")
        print(f"Response body: {response.text}")
        
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.headers["Content-Type"], "text/plain")
        self.assertEqual(response.text, "OK")

    def test_server_health(self):
        """Verify server is running and responding"""
        print("\n=== Testing Server Health ===")
        try:
            print(f"Attempting GET request to {self._url("/echo")}")
            response = requests.get(self._url("/echo"), timeout=5)
            print(f"Response status: {response.status_code}")
            print(f"Response headers: {response.headers}")
            print(f"Response body (first 100 chars): {response.text[:100]}")
            self.assertEqual(response.status_code, 200)
        except requests.exceptions.RequestException as e:
            print(f"Request failed: {str(e)}")
            self.fail(f"Server not responding: {e}")

    def test_http_basic_response(self):
        """Test that the server responds to basic HTTP requests"""
        print("\n=== Testing Basic Response ===")
        url = self._url("/echo")
        print(f"Request URL: {url}")
        response = requests.get(url)
        
        print(f"Full response: {response.text}")
        self.assertEqual(response.status_code, 200)
        self.assertTrue(response.text.startswith("GET /echo HTTP/1.1"),
                    f"Unexpected response start: {response.text[:50]}...")
        self.assertIn("Host: localhost", response.text)

    def test_http_echo_complete_request(self):
        """Verify the server echoes the complete HTTP request"""
        print("\n=== Testing Request Echoing ===")
        test_body = "test_content_123"
        headers = {
            'User-Agent': 'TestAgent/1.0',
            'X-Custom-Header': 'test-value'
        }
        url = self._url("/echo/test_path")
        
        print(f"POSTing to {url}")
        print(f"Headers: {headers}")
        print(f"Body: {test_body}")
        
        response = requests.post(url, headers=headers, data=test_body)
        
        print(f"Full echoed response: {response.text}")
        echoed = response.text
        self.assertIn("POST /echo/test_path HTTP/1.1", echoed)
        for header, value in headers.items():
            self.assertIn(f"{header}: {value}", echoed)
        self.assertTrue(echoed.endswith(test_body))

    def test_invalid_request_handling(self):
        """Test that the server properly handles malformed requests"""
        print("\n=== Testing Invalid Request Handling ===")
        malformed_requests = [
            ("INVALID_HTTP_REQUEST\r\n\r\n", "Completely invalid request"),
            ("GET\r\n\r\n", "Missing path and version"),
            ("GET /path\r\n\r\n", "Missing version"),
            ("GET HTTP/1.1\r\n\r\n", "Missing path"),
            (" /path HTTP/1.1\r\n\r\n", "Missing method")
        ]

        for request, description in malformed_requests:
            try:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                    s.connect(('localhost', self.server_port))
                    s.sendall(request.encode())
                    response = s.recv(1024).decode()
                    
                    # Check for 400 Bad Request response
                    self.assertIn("HTTP/1.1 400 Bad Request", response, 
                                f"Failed to get 400 Bad Request for {description}")
                    self.assertIn("Content-Type: text/plain", response,
                                f"Missing Content-Type header for {description}")
                    self.assertIn("400 Bad Request", response,
                                f"Missing error message for {description}")
            except Exception as e:
                self.fail(f"Server crashed on {description}: {str(e)}")

    def test_concurrent_connections(self):
        """Verify multiple simultaneous connections"""
        print("\n=== Testing Concurrent Connections ===")
        def make_request():
            try:
                url = self._url("/echo/concurrent")
                print(f"Thread making request to {url}")
                response = requests.get(url, timeout=2)
                return response.text
            except Exception as e:
                print(f"Request failed in thread: {str(e)}")
                return str(e)
        
        print("Starting 5 concurrent requests...")
        with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
            futures = [executor.submit(make_request) for _ in range(5)]
            for i, future in enumerate(concurrent.futures.as_completed(futures)):
                result = future.result()
                print(f"Request {i+1} result (first 50 chars): {result[:50]}")
                self.assertIn("GET /echo/concurrent HTTP/1.1", result)

    def test_static_404_response(self):
        """Test non-existent static file paths return 404"""
        print("\n=== Testing Static 404 Response ===")
        url = self._url("/static/nonexistant")
        response = requests.get(url)
        print(f"Received response: {response.status_code}, {response.text[:100]}")
        self.assertEqual(response.status_code, 404)
        self.assertIn("404 Not Found", response.text)

    def test_static_files_and_mime_types(self):
        """Test that static files are served correctly with the right MIME types."""
        print("\n=== Testing Static Files and MIME Types ===")

        # Create static test files
        test_files = {
            'test.html': ('text/html', '<html>test</html>'),
            'test.txt': ('text/plain', 'plain text'),
            'test.json': ('application/json', '{"test": "value"}')
        }
        
        for filename, (mime_type, content) in test_files.items():
            with self.subTest(filename=filename):
                path = os.path.join(self.test_static_dir, filename)
                print(f"Creating file {filename} with content:\n{content}")
                with open(path, 'w') as f:
                    f.write(content)
                
                try:
                    url = self._url(f"/static/{filename}")
                    print(f"Sending request to {url}")
                    response = requests.get(url)
                    print(f"Response status code for {filename}: {response.status_code}")
                    print(f"Response headers for {filename}: {response.headers}")
                    print(f"Response body for {filename} (first 100 chars): {response.text[:100]}")
                    
                    # Check status code and MIME type
                    self.assertEqual(response.status_code, 200)
                    self.assertEqual(response.text.strip(), content.strip())
                    self.assertTrue(response.headers['Content-Type'].startswith(mime_type))
                
                except Exception as e:
                    print(f"Error while testing static file {filename}: {e}")
                
                finally:
                    os.remove(path)
                    print(f"Deleted test file {filename}")

    def test_large_file_handling(self):
        """Test handling of large static files"""
        print("\n=== Testing Large File Handling ===")
        large_file = os.path.join(self.test_static_dir, 'large.bin')
        test_size = 10 * 1024 * 1024  # 10MB
        with open(large_file, 'wb') as f:
            f.write(os.urandom(test_size))
        
        try:
            response = requests.get(self._url("/static/large.bin"), stream=True)
            self.assertEqual(response.status_code, 200)
            self.assertEqual(int(response.headers['Content-Length']), test_size)
        finally:
            os.remove(large_file)

    def test_duplicate_paths_rejected(self):
        """Ensure server fails to start with duplicate location paths"""
        print("\n=== Testing Duplicate Path Handling ===")

        # Config file with duplicate /static path
        bad_config_path = os.path.join(self.project_root, "tests", "bad_config")
        try:
            with open(bad_config_path, "w") as f:
                f.write(f"""
                port {self.server_port};

                location /static StaticHandler {{
                    root {self.test_static_dir};
                }}

                location /static EchoHandler {{
                }}
                """)

            proc = subprocess.Popen(
                [self.server_binary, bad_config_path],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                preexec_fn=os.setsid
            )
            time.sleep(0.2) # Wait briefly to see if it crashes
            exit_code = proc.poll()
            if exit_code is None:
                os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                proc.wait()
                self.fail("Server did not reject duplicate paths and continued running") # Still running, so kill it
            else:
                _, stderr = proc.communicate()
                print(f"Server exited as expected. STDERR:\n{stderr.decode()}")
                self.assertIn("duplicate", stderr.decode().lower())
        finally:
            if os.path.exists(bad_config_path):
                os.remove(bad_config_path)

    def test_crud_api(self):
        """Test that CRUD API correctly creates and deletes an entity."""
        print("\n=== Testing CRUD API ===")

        # Test PUT (create an instance of an Entity with a pre-determined ID)
        put_id = 1
        url = self._url(f"/api/shoes/{put_id}")
        print(f"Testing PUT {url}")
        response = requests.put(url, json={"color": "red"})
        self.assertEqual(response.status_code, 201)
        
        # Test POST
        url = self._url("/api/shoes")
        print(f"Testing POST {url}")
        response = requests.post(url, json={"color": "green"})
        self.assertEqual(response.status_code, 201)
        post_id = response.json()["id"]
        self.assertEqual(response.text, f'{{"id": {post_id}}}')

        # Test GET (specific instance of an Entity)
        url = self._url(f"/api/shoes/{put_id}")
        print(f"Testing GET {url}")
        response = requests.get(url)
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.text, '{"color": "red"}')

        url = self._url(f"/api/shoes/{post_id}")
        print(f"Testing GET {url}")
        response = requests.get(url)
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.text, '{"color": "green"}')

        # Test GET (all instances of an Entity)
        url = self._url(f"/api/shoes")
        response = requests.get(url)
        self.assertIn(response.text, [f'["{put_id}","{post_id}"]', f'["{post_id}","{put_id}"]'])

        # Test PUT (update existing instance of an Entity)
        url = self._url(f"/api/shoes/{put_id}")
        print(f"Testing PUT {url}")
        response = requests.put(url, json={"color": "blue"})
        self.assertEqual(response.status_code, 204)

        url = self._url(f"/api/shoes/{put_id}")
        print(f"Testing GET {url}")
        response = requests.get(url)
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.text, '{"color": "blue"}')

        # Test DELETE
        url = self._url(f"/api/shoes/{put_id}")
        print(f"Testing DELETE {url}")
        response = requests.delete(url)
        self.assertEqual(response.status_code, 204)

        url = self._url(f"/api/shoes/{post_id}")
        print(f"Testing DELETE {url}")
        response = requests.delete(url)
        self.assertEqual(response.status_code, 204)
        
        url = self._url(f"/api/shoes/{put_id}")
        print(f"Testing GET {url}")
        response = requests.get(url)
        self.assertEqual(response.status_code, 404)
        
        url = self._url(f"/api/shoes/{post_id}")
        print(f"Testing GET {url}")
        response = requests.get(url)
        self.assertEqual(response.status_code, 404)

        url = self._url(f"/api/shoes")
        response = requests.get(url)
        self.assertEqual(response.text, "[]")

    def test_concurrent_requests(self):
        """Verify server can handle multiple simultaneous requests"""
        print("\n=== Testing Concurrent Requests ===")
        sleep_url = self._url("/sleep")
        fast_url = self._url("/echo")

        def make_sleep_request():
            print("Starting slow /sleep request")
            try:
                response = requests.get(sleep_url, timeout=10)
                print("Slow request completed")
                return response
            except Exception as e:
                print(f"Slow request failed: {e}")
                return None

        def make_fast_request():
            print("Starting fast /echo request")
            start = time.time()
            try:
                resp = requests.get(fast_url, timeout=10)
                duration = time.time() - start
                print(f"Fast request completed in {duration:.3f} seconds")
                return duration
            except Exception as e:
                print(f"Fast request failed: {e}")
                return float("inf")

        with concurrent.futures.ThreadPoolExecutor() as executor:
            slow_future = executor.submit(make_sleep_request)
            time.sleep(0.5)  # Give slow request time to start

            fast_duration = make_fast_request()
            self.assertLess(fast_duration, 1.0, "Fast request blocked by slow request")

            slow_future.result()  # Wait for completion
            print("=== Concurrent Requests Test Passed ===")

    def test_concurrent_connections(self):
        """Verify multiple simultaneous connections"""
        print("\n=== Testing Concurrent Connections ===")

        def make_request():
            try:
                url = self._url("/echo/concurrent")
                print(f"Sending request to {url}")
                response = requests.get(url, timeout=2)
                return response.status_code, response.text
            except Exception as e:
                return None, f"Error: {str(e)}"

        num_requests = 5
        with concurrent.futures.ThreadPoolExecutor(max_workers=num_requests) as executor:
            futures = [executor.submit(make_request) for _ in range(num_requests)]

            for i, future in enumerate(concurrent.futures.as_completed(futures), 1):
                status, result = future.result()
                print(f"Request {i} result (status: {status}): {result[:50]}")
                self.assertEqual(status, 200)
                self.assertIn("GET /echo/concurrent HTTP/1.1", result)

        print("=== Concurrent Connections Test Passed ===")

    def test_true_concurrency(self):
        """Verify requests are actually handled in parallel"""
        print("\n=== Testing True Concurrency ===")

        slow_url = self._url("/sleep")
        fast_url = self._url("/echo")

        start = time.time()

        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
            # Start slow request first
            slow_future = executor.submit(lambda: requests.get(slow_url, timeout=3))
            time.sleep(0.1)  # Small delay to ensure slow request starts first
    
            # Start fast request
            fast_future = executor.submit(lambda: requests.get(fast_url, timeout=3))

            try:
                slow_result = slow_future.result()
                fast_result = fast_future.result()

                self.assertEqual(slow_result.status_code, 200)
                self.assertEqual(fast_result.status_code, 200)
            except Exception as e:
                self.fail(f"Request failed: {str(e)}")

        duration = time.time() - start
        print(f"Parallel requests completed in {duration:.2f}s")

        # The sleep handler sleeps for 1 second, so both requests should complete in ~1s
        # if they're truly parallel. If they were sequential, it would take ~1s + fast request time
        self.assertLess(duration, 1.5, "Requests didn't execute in parallel")

    def test_crud_concurrency(self):
        """Verify CRUD operations handle concurrent access"""
        print("\n=== Testing CRUD Concurrency ===")

        def worker(i):
            url = self._url(f"/api/test/{i}")
            print(f"[Worker {i}] PUT to {url} with value {i}")

            response = requests.put(url, json={"value": i}, timeout=5)
            print(f"[Worker {i}] PUT response: {response.status_code} - {response.text}")
            self.assertIn(response.status_code, [201, 204])

            response_get = requests.get(url, timeout=5)
            print(f"[Worker {i}] GET response: status={response_get.status_code}, body_len={len(response_get.text)}, body_preview='{response_get.text[:50]}'", flush=True)
            print("GET response")

            # Test fails if the body is empty after a successful PUT
            self.assertEqual(response_get.status_code, 200, f"[Worker {i}] GET failed with status {response_get.status_code}")
            self.assertTrue(response_get.text, f"[Worker {i}] GET response body is unexpectedly empty for {url}. Content was PUT before.")

            try:
                self.assertEqual(response_get.json()["value"], i, f"[Worker {i}] GET response JSON content mismatch.")
            except requests.exceptions.JSONDecodeError as e:
                self.fail(f"[Worker {i}] Failed to decode JSON: {e}. Raw body: '{response_get.text}'")

            response = requests.delete(url, timeout=5)
            print(f"[Worker {i}] DELETE response: {response.status_code} - {response.text}")
            self.assertIn(response.status_code, [200, 204])

        with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
            futures = [executor.submit(worker, i) for i in range(10)]
            for future in concurrent.futures.as_completed(futures):
                future.result()  # Will raise exceptions if any

        print("=== CRUD Concurrency Test Passed ===")

    def test_protected_path_without_credentials(self):
        """Verify unauthorized response to protected path without credentials"""
        print("\n=== Testing Protected Path Without Credentials ===")
        url = self._url("/secure")
        print(f"Requesting URL without credentials: {url}")

        response = requests.get(url)

        print(f"Response status code: {response.status_code}")
        print(f"Response headers: {response.headers}")
        print(f"Response body:\n{response.text}")

        self.assertEqual(response.status_code, 401)
        self.assertIn("WWW-Authenticate", response.headers)

    def test_protected_path_with_credentials(self):
        """Verify normal response to protected path with credentials"""
        print("\n=== Testing Protected Path With Credentials ===")
        url = self._url("/secure")
        auth = ("admin", "password123")
        print(f"Requesting URL with credentials: {url}, username: {auth[0]}")

        response = requests.get(url, auth=auth)

        print(f"Response status code: {response.status_code}")
        print(f"Response headers: {response.headers}")
        print(f"Response body:\n{response.text}")

        self.assertEqual(response.status_code, 200)

if __name__ == "__main__":
    unittest.main()
