import React, { useEffect, useState, useRef } from "react";
import { io } from "socket.io-client";

export default function Chat({ username }) {
  const [socket, setSocket] = useState(null);
  const [users, setUsers] = useState([]);
  const [selected, setSelected] = useState(null);
  const [messages, setMessages] = useState([]);
  const [text, setText] = useState("");
  const msgsRef = useRef(null);

  useEffect(() => {
    const s = io("http://localhost:4000", { auth: { username } });
    setSocket(s);

    s.on("connect_error", (err) => console.error(err));
    s.on("users", (list) => setUsers(list.filter((u) => u !== username)));
    s.on("message", (m) => setMessages((prev) => [...prev, m]));

    return () => s.disconnect();
  }, [username]);

  useEffect(() => {
    if (msgsRef.current) {
      msgsRef.current.scrollTop = msgsRef.current.scrollHeight;
    }
  }, [messages]);

  const send = () => {
    if (!text.trim() || !selected) return;
    socket.emit("private_message", { to: selected, content: text });
    setMessages((prev) => [
      ...prev,
      { id: Date.now(), sender: username, content: text, timestamp: Date.now() },
    ]);
    setText("");
  };

  return (
    <div style={{ display: "flex", height: "100vh" }}>
      <div
        style={{
          width: 200,
          borderRight: "1px solid #ccc",
          padding: 10,
          boxSizing: "border-box",
        }}
      >
        <h3>{username}</h3>
        <h4>Users</h4>
        <ul>
          {users.map((u) => (
            <li
              key={u}
              style={{ cursor: "pointer", fontWeight: selected === u ? "bold" : "normal" }}
              onClick={() => setSelected(u)}
            >
              {u}
            </li>
          ))}
        </ul>
      </div>
      <div style={{ flex: 1, display: "flex", flexDirection: "column" }}>
        <div
          ref={msgsRef}
          style={{ flex: 1, padding: 10, overflowY: "auto", borderBottom: "1px solid #ccc" }}
        >
          {messages.map((m) => (
            <div key={m.id} style={{ marginBottom: 10 }}>
              <b>{m.sender}:</b> {m.content}
            </div>
          ))}
        </div>
        <div style={{ display: "flex", padding: 10 }}>
          <input
            style={{ flex: 1, marginRight: 5 }}
            value={text}
            onChange={(e) => setText(e.target.value)}
            placeholder={selected ? `Message ${selected}` : "Select a user"}
            disabled={!selected}
            onKeyDown={(e) => e.key === "Enter" && send()}
          />
          <button onClick={send} disabled={!selected}>
            Send
          </button>
        </div>
      </div>
    </div>
  );
}