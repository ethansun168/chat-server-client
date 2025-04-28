INSERT INTO users (username, password) VALUES
    ('user1', 'password1'),
    ('user2', 'password2'),
    ('user3', 'password3');

INSERT INTO messages (sender_id, receiver_id, message, timestamp) VALUES
    (1, 2, 'Hello, user2!', 0),
    (2, 1, 'Hello, user1!', 0),
    (1, 3, 'Hello, user3!', 0),
    (3, 1, 'Hello, user1!', 0);

INSERT INTO global_messages (sender_id, message, timestamp) VALUES
    (3, 'Global message 3', 0),
    (1, 'Global message 1', 1),
    (2, 'Global message 2', 2);