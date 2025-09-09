// small helpers
export const API_BASE = 'http://localhost:4000';

export async function login(username){
  const res = await fetch(API_BASE + '/auth/login', {
    method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ username })
  });
  return res.json();
}

export async function fetchUsers(){
  const res = await fetch(API_BASE + '/users');
  return res.json();
}

export async function fetchMessages(token, other){
  const res = await fetch(API_BASE + '/messages/' + encodeURIComponent(other), { headers: { Authorization: 'Bearer ' + token } });
  return res.json();
}