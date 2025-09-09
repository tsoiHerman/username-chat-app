import express from 'express';
import http from 'http';
import { Server } from 'socket.io';

const app = express();
const server = http.createServer(app);
const io = new Server(server, { cors: { origin: "*" } });

let onlineUsers = {}; // username -> socket.id

io.on('connection', (socket) => {
  const { username } = socket.handshake.auth;
  if (!username) return;

  onlineUsers[username] = socket.id;

  // send updated user list to everyone
  io.emit('users', Object.keys(onlineUsers));

  socket.on('private_message', ({ to, content }) => {
    const targetSocketId = onlineUsers[to];
    if (targetSocketId) {
      io.to(targetSocketId).emit('message', {
        id: Date.now(),
        sender: username,
        content,
        timestamp: Date.now(),
      });
    }
  });

  socket.on('disconnect', () => {
    delete onlineUsers[username];
    io.emit('users', Object.keys(onlineUsers));
  });
});

server.listen(4000, () => console.log("Server listening on port 4000"));