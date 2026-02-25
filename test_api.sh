#!/bin/bash

# Bash script to test the Chat API

BASE_URL="http://localhost:8080"

echo "=== Testing Chat API ==="

# Test 1: Health Check
echo -e "\n1. Health Check..."
curl -s "$BASE_URL/api/health" | jq '.'
echo "✓ Health check complete"

# Test 2: Send first message
echo -e "\n2. Sending first message..."
curl -s -X POST "$BASE_URL/api/messages" \
  -H "Content-Type: application/json" \
  -d '{"username":"alice","content":"Hello, this is my first message!"}' | jq '.'
echo "✓ First message sent"

# Test 3: Send second message
echo -e "\n3. Sending second message..."
curl -s -X POST "$BASE_URL/api/messages" \
  -H "Content-Type: application/json" \
  -d '{"username":"bob","content":"Hi Alice! Nice to meet you."}' | jq '.'
echo "✓ Second message sent"

# Test 4: Get all messages
echo -e "\n4. Retrieving all messages..."
curl -s "$BASE_URL/api/messages" | jq '.'
echo "✓ All messages retrieved"

# Test 5: Get specific message
echo -e "\n5. Getting message by ID (1)..."
curl -s "$BASE_URL/api/messages/1" | jq '.'
echo "✓ Message retrieved"

# Test 6: Test invalid request (missing fields)
echo -e "\n6. Testing invalid request (should fail)..."
curl -s -X POST "$BASE_URL/api/messages" \
  -H "Content-Type: application/json" \
  -d '{"username":"charlie"}' | jq '.'
echo "✓ Invalid request correctly rejected"

echo -e "\n=== Testing Complete ==="



