<?php
/**
 * User Web Application
 * 
 * This is a SAMPLE user application showing how to use HybridDB
 * Users can register, login, and use their dashboard
 */

session_start();
require_once '../../client-libs/php/HybridDB.php';

// Initialize HybridDB connection
$db = new HybridDB('localhost', 5432);

// Create users table if not exists
try {
    $db->query("CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY,
        username STRING NOT NULL,
        password STRING NOT NULL,
        email STRING NOT NULL,
        created_at TIMESTAMP
    )");
} catch (Exception $e) {
    // Table already exists
}

// Handle registration
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['register'])) {
    $username = $_POST['username'] ?? '';
    $password = $_POST['password'] ?? '';
    $email = $_POST['email'] ?? '';

    if ($username && $password && $email) {
        $passwordHash = password_hash($password, PASSWORD_DEFAULT);

        try {
            $db->insert('users', [
                'username' => $username,
                'password' => $passwordHash,
                'email' => $email,
                'created_at' => time()
            ]);

            $success = "Registration successful! Please login.";
        } catch (Exception $e) {
            $error = "Registration failed: " . $e->getMessage();
        }
    }
}

// Handle login
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['login'])) {
    $username = $_POST['username'] ?? '';
    $password = $_POST['password'] ?? '';

    if ($username && $password) {
        try {
            $users = $db->select('users', "username = '$username'");

            if (!empty($users) && password_verify($password, $users[0]['password'])) {
                $_SESSION['user_id'] = $users[0]['id'];
                $_SESSION['username'] = $users[0]['username'];
                header('Location: dashboard.php');
                exit;
            } else {
                $error = "Invalid username or password";
            }
        } catch (Exception $e) {
            $error = "Login failed: " . $e->getMessage();
        }
    }
}

// Check if logged in
$loggedIn = isset($_SESSION['user_id']);

?>
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>HybridDB User Application</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .container {
            background: white;
            padding: 40px;
            border-radius: 10px;
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.2);
            max-width: 400px;
            width: 100%;
        }

        h1 {
            color: #333;
            margin-bottom: 10px;
            text-align: center;
        }

        .subtitle {
            color: #666;
            text-align: center;
            margin-bottom: 30px;
        }

        .tabs {
            display: flex;
            margin-bottom: 20px;
            border-bottom: 2px solid #eee;
        }

        .tab {
            flex: 1;
            padding: 10px;
            text-align: center;
            cursor: pointer;
            border-bottom: 2px solid transparent;
            margin-bottom: -2px;
            transition: all 0.3s;
        }

        .tab.active {
            border-bottom-color: #667eea;
            color: #667eea;
            font-weight: bold;
        }

        .tab-content {
            display: none;
        }

        .tab-content.active {
            display: block;
        }

        .form-group {
            margin-bottom: 20px;
        }

        label {
            display: block;
            margin-bottom: 5px;
            color: #333;
            font-weight: 500;
        }

        input {
            width: 100%;
            padding: 12px;
            border: 1px solid #ddd;
            border-radius: 5px;
            font-size: 14px;
        }

        input:focus {
            outline: none;
            border-color: #667eea;
        }

        button {
            width: 100%;
            padding: 12px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 5px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: transform 0.3s;
        }

        button:hover {
            transform: translateY(-2px);
        }

        .alert {
            padding: 10px;
            border-radius: 5px;
            margin-bottom: 20px;
        }

        .alert-success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }

        .alert-error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }

        .info {
            background: #e8f4f8;
            padding: 15px;
            border-radius: 5px;
            margin-top: 20px;
            text-align: center;
            color: #31708f;
        }
    </style>
</head>

<body>
    <div class="container">
        <h1>🗄️ HybridDB</h1>
        <div class="subtitle">User Application Demo</div>

        <?php if (isset($error)): ?>
            <div class="alert alert-error">
                <?php echo htmlspecialchars($error); ?>
            </div>
        <?php endif; ?>

        <?php if (isset($success)): ?>
            <div class="alert alert-success">
                <?php echo htmlspecialchars($success); ?>
            </div>
        <?php endif; ?>

        <?php if ($loggedIn): ?>
            <div class="alert alert-success">
                You are logged in as <strong>
                    <?php echo htmlspecialchars($_SESSION['username']); ?>
                </strong>
            </div>
            <a href="dashboard.php"><button>Go to Dashboard</button></a>
            <div style="margin-top: 10px;">
                <a href="logout.php"><button style="background: #dc3545;">Logout</button></a>
            </div>
        <?php else: ?>
            <div class="tabs">
                <div class="tab active" onclick="switchTab('login')">Login</div>
                <div class="tab" onclick="switchTab('register')">Register</div>
            </div>

            <div class="tab-content active" id="login-tab">
                <form method="POST">
                    <div class="form-group">
                        <label>Username</label>
                        <input type="text" name="username" required>
                    </div>
                    <div class="form-group">
                        <label>Password</label>
                        <input type="password" name="password" required>
                    </div>
                    <button type="submit" name="login">Login</button>
                </form>
            </div>

            <div class="tab-content" id="register-tab">
                <form method="POST">
                    <div class="form-group">
                        <label>Username</label>
                        <input type="text" name="username" required>
                    </div>
                    <div class="form-group">
                        <label>Email</label>
                        <input type="email" name="email" required>
                    </div>
                    <div class="form-group">
                        <label>Password</label>
                        <input type="password" name="password" required>
                    </div>
                    <button type="submit" name="register">Register</button>
                </form>
            </div>
        <?php endif; ?>

        <div class="info">
            <strong>Note:</strong> This app uses HybridDB - a C++ database server.<br>
            All database logic runs in C++, this is just a thin PHP client.
        </div>
    </div>

    <script>
        function switchTab(tab) {
            document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));

            event.target.classList.add('active');
            document.getElementById(tab + '-tab').classList.add('active');
        }
    </script>
</body>

</html>