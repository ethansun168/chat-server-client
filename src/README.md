# Protocols
## Message Type: CHAT
```
sender: message sender
receiver: message receiver
content: content of the chat message
token: client token
timestamp: timestamp of the message
```
**Global Chat**
```
sender: message sender
receiver: ""
content: content of the chat message
token: client token
timestamp: timestamp of the message
```
Response sent to online users:
```
sender: message sender
receiver: message receiver
content: content of the chat message
token: client token
timestamp: timestamp of the message
```

## Message Type: AUTH
```
sender: username
receiver: password 
content: ""
token: ""
timestamp: timestamp of the message
```
Response
```
sender: username
receiver: password 
content: ""
token: 32 character token
timestamp: timestamp of the message
```

## Message Type: COMMAND
```
sender: ""
receiver: ""
content: {allUsers, onlineUsers}
token: client token
timestamp: timestamp of the message
```
```
sender: sender
receiver: receiver
content: chat
token: client token
timestamp: timestamp of the message
```
Responses:
```
allUsers: list of all users separated by \n (in content)
onlineUsers: list of online users separated by \n (in content)
chat: list of all chat messages in the following format in the content field (note the newline between each entry)
    timestamp sender message
    timestamp sender message
```
**Global Chat**
```
sender: sender
receiver: ""
content: globalChat
token: client token
timestamp: timestamp of the message
```
Response
```
content: list of all chat messages in the global chatroom
    timestamp sender message
```

If authentication fails, token will be empty