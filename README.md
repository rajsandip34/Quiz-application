# Small Wins Quiz

A lightweight multiple-choice quiz with a browser interface and a C socket server. The server owns the questions and checks the score.

## Run it

### Windows (MinGW GCC)

```powershell
gcc server.c -o quiz-server.exe -lws2_32
.\quiz-server.exe
```

### Windows (Visual Studio Developer Command Prompt)

```bat
cl /W4 server.c ws2_32.lib
server.exe
```

### macOS or Linux

```sh
cc -std=c11 -Wall -Wextra server.c -o quiz-server
./quiz-server
```

Open <http://localhost:8080>. Keep `server.c`, `index.html`, `styles.css`, and `app.js` in the same folder while the server runs. Press Ctrl+C to stop it.

## API

- `GET /api/questions` returns question text and choices, without the answer key.
- `POST /api/submit` accepts `{"answers":[1,1,3,1,2]}` and returns the score, correct choices, and explanations.

The questions and answer key are in the `questions` array near the top of `server.c`. This is a small learning project; the server handles one request at a time and stores no player data.
