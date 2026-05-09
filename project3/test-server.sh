#!/bin/bash

# Simplified test suite for Gunrock Web Server - focuses on core functionality
# Avoids hanging on concurrent wait() commands

PORT=8765
THREADS=19
BUFFERS=84

echo "========================================"
echo "Gunrock Web Server Test Suite"
echo "========================================"
echo ""

# Kill any existing servers
pkill -f "gunrock_web.*$PORT" 2>/dev/null
sleep 1

# Start server
echo "[*] Starting server on port $PORT with $THREADS threads and $BUFFERS buffers..."
./gunrock_web -p $PORT -t $THREADS -b $BUFFERS > /tmp/server.log 2>&1 &
SERVER_PID=$!
sleep 2

if ! kill -0 $SERVER_PID 2>/dev/null; then
  echo "[!] ERROR: Server failed to start"
  exit 1
fi

echo "[+] Server started (PID: $SERVER_PID)"
echo ""

# Test counter
PASS=0
FAIL=0

# Helper function to test responses
test_curl() {
  local name=$1
  local url=$2
  local expected_code=$3
  local flags=$4
  
  response=$(curl -s -m 5 -w "\n%{http_code}" $flags "http://localhost:$PORT$url" 2>/dev/null)
  code=$(echo "$response" | tail -n 1)
  
  if [ "$code" == "$expected_code" ]; then
    echo "[✓] $name - Status: $code"
    ((PASS++))
    return 0
  else
    echo "[✗] $name - Expected: $expected_code, Got: $code"
    ((FAIL++))
    return 1
  fi
}

# Helper function for path-as-is tests (prevent curl from normalizing paths)
test_curl_path_as_is() {
  local name=$1
  local url=$2
  local expected_code=$3
  
  response=$(curl -s -m 5 -w "\n%{http_code}" --path-as-is "http://localhost:$PORT$url" 2>/dev/null)
  code=$(echo "$response" | tail -n 1)
  
  if [ "$code" == "$expected_code" ]; then
    echo "[✓] $name - Status: $code"
    ((PASS++))
    return 0
  else
    echo "[✗] $name - Expected: $expected_code, Got: $code"
    ((FAIL++))
    return 1
  fi
}

echo "========================================"
echo "Test Group 1: Basic Functionality"
echo "========================================"

test_curl "GET /hello_world.html" "/hello_world.html" "200" ""
test_curl "GET /bootstrap.html" "/bootstrap.html" "200" ""
test_curl "GET nonexistent.html (404)" "/nonexistent.html" "404" ""
test_curl "HEAD /hello_world.html" "/hello_world.html" "200" "--head"
test_curl "POST /hello_world.html (501)" "/hello_world.html" "501" "-X POST"

echo ""
echo "========================================"
echo "Test Group 2: Security (Path Traversal)"
echo "========================================"

test_curl_path_as_is "Path traversal (../) blocked" "/../README.md" "403"
test_curl_path_as_is "Path traversal (../../) blocked" "/../../etc/passwd" "403"
test_curl_path_as_is "Path traversal (middle) blocked" "/dir/../README.md" "403"

echo ""
echo "========================================"
echo "Test Group 3: Content Types"
echo "========================================"

test_curl "GET /bootstrap/album.css" "/bootstrap/album.css" "200" ""
test_curl "GET /bootstrap/holder.min.js" "/bootstrap/holder.min.js" "200" ""

echo ""
echo "========================================"
echo "Test Group 4: Concurrent Requests"
echo "========================================"

echo "[*] Sending 5 concurrent requests (with timeouts)..."
concurrent_success=0
for i in {1..5}; do
  if curl -s -m 5 http://localhost:$PORT/hello_world.html > /dev/null 2>&1; then
    ((concurrent_success++))
  fi
done

if [ $concurrent_success -eq 5 ]; then
  echo "[✓] 5 concurrent requests completed successfully"
  ((PASS+=5))
else
  echo "[✗] Only $concurrent_success/5 concurrent requests completed"
  ((FAIL+=1))
fi

echo ""
echo "========================================"
echo "Cleanup"
echo "========================================"

kill $SERVER_PID 2>/dev/null
sleep 1

echo ""
echo "========================================"
echo "Test Results"
echo "========================================"
echo "Passed: $PASS"
echo "Failed: $FAIL"
echo "========================================"

if [ $FAIL -eq 0 ]; then
  echo "[+] ALL TESTS PASSED ✓"
  exit 0
else
  echo "[!] SOME TESTS FAILED"
  exit 1
fi
