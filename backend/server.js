import express from "express";
import http from "http";
import { Server } from "socket.io";

const app = express();
const server = http.createServer(app);

const io = new Server(server, {
  cors: {
    origin: [
      "https://username-chat-frontend.vercel.app", // your Vercel URL
      "http://localhost:5173"                     // for local dev
    ],
    methods: ["GET", "POST"]
  },
  transports: ["websocket", "polling"], // ensure WebSocket transport
});

const users = new Map();

io.on("connection", (socket) => {
  const { username } = socket.handshake.auth;
  if (!username) return;

  users.set(username, socket.id);
  io.emit("users", Array.from(users.keys()));

  socket.on("private_message", (msg) => {
    const recipientSocketId = users.get(msg.to);
    if (recipientSocketId) io.to(recipientSocketId).emit("message", msg);
    socket.emit("message", msg);
  });

  socket.on("disconnect", () => {
    users.delete(username);
    io.emit("users", Array.from(users.keys()));
  });
});

const PORT = process.env.PORT || 4000;
server.listen(PORT, () => console.log("Server running on port", PORT));