SELECT sender_id, username, message, timestamp FROM global_messages g
JOIN users u ON g.sender_id = u.id;