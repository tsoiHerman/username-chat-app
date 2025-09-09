import React, { useState } from "react";
import Chat from "./Chat.jsx";

export default function App() {
  const [username, setUsername] = useState("");
  const [loggedIn, setLoggedIn] = useState(false);

  const login = () => {
    if (username.trim()) setLoggedIn(true);
  };

  if (!loggedIn) {
    return (
      <div style={{ padding: 20 }}>
        <h2>Enter a username</h2>
        <input
          value={username}
          onChange={(e) => setUsername(e.target.value)}
          placeholder="Your username"
        />
        <button onClick={login}>Enter</button>
      </div>
    );
  }

  return <Chat username={username} />;
}