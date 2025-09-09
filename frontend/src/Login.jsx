import React, { useState } from 'react'
import { login } from './api'

export default function Login({ onLogin }){
  const [username, setUsername] = useState('')
  const [err, setErr] = useState(null)
  async function submit(e){
    e.preventDefault();
    if (!username.trim()) return setErr('Enter a username')
    const { token, username: user, error } = await login(username.trim());
    if (error) return setErr(error)
    localStorage.setItem('token', token);
    localStorage.setItem('username', user);
    onLogin({ token, username: user });
  }
  return (
    <div style={{padding:20}}>
      <h2>Sign in (username only)</h2>
      <form onSubmit={submit}>
        <input value={username} onChange={e=>setUsername(e.target.value)} placeholder="Choose a username" />
        <button type="submit">Enter</button>
      </form>
      {err && <div style={{color:'red'}}>{err}</div>}
      <p style={{fontSize:'12px',color:'#666'}}>No password. Username must be unique to create account.</p>
    </div>
  )
}