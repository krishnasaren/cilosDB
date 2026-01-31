<?php
session_start();
require_once '../../client-libs/php/HybridDB.php';

if (!isset($_SESSION['user_id'])) {
    header('Location: index.php');
    exit;
}

$db = new HybridDB('localhost', 5432);
$username = $_SESSION['username'];
?>
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Dashboard - HybridDB User App</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #f5f5f5;
        }

        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px 40px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .header h1 {
            font-size: 24px;
        }

        .user-info {
            display: flex;
            align-items: center;
            gap: 20px;
        }

        .container {
            max-width: 1200px;
            margin: 40px auto;
            padding: 0 20px;
        }

        .card {
            background: white;
            border-radius: 10px;
            padding: 30px;
            box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
            margin-bottom: 20px;
        }

        .card h2 {
            margin-bottom: 20px;
            color: #333;
        }

        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-bottom: 40px;
        }

        .stat-card {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            border-radius: 10px;
            text-align: center;
        }

        .stat-value {
            font-size: 36px;
            font-weight: bold;
            margin-bottom: 10px;
        }

        .stat-label {
            font-size: 14px;
            opacity: 0.9;
        }

        .btn {
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 14px;
            transition: all 0.3s;
        }

        .btn-primary {
            background: #667eea;
            color: white;
        }

        .btn-danger {
            background: #dc3545;
            color: white;
        }

        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.2);
        }

        table {
            width: 100%;
            border-collapse: collapse;
        }

        th,
        td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }

        th {
            background: #f8f9fa;
            font-weight: 600;
        }
    </style>
</head>

<body>
    <div class="header">
        <h1>🗄️ HybridDB Dashboard</h1>
        <div class="user-info">
            <span>Welcome, <strong>
                    <?php echo htmlspecialchars($username); ?>
                </strong></span>
            <a href="logout.php"><button class="btn btn-danger">Logout</button></a>
        </div>
    </div>

    <div class="container">
        <div class="stats-grid">
            <div class="stat-card">
                <div class="stat-value">1</div>
                <div class="stat-label">Active Session</div>
            </div>
            <div class="stat-card">
                <div class="stat-value">C++</div>
                <div class="stat-label">Database Engine</div>
            </div>
            <div class="stat-card">
                <div class="stat-value">Fast</div>
                <div class="stat-label">Performance</div>
            </div>
        </div>

        <div class="card">
            <h2>Welcome to Your Dashboard</h2>
            <p>This is a sample user application built with HybridDB.</p>
            <br>
            <p><strong>Technology Stack:</strong></p>
            <ul style="margin-left: 20px; line-height: 1.8;">
                <li><strong>Database:</strong> C++ HybridDB Server (port 5432)</li>
                <li><strong>Client Library:</strong> PHP Thin Client (socket communication)</li>
                <li><strong>Frontend:</strong> PHP + HTML + CSS</li>
                <li><strong>Authentication:</strong> PHP Sessions + password_hash()</li>
            </ul>
        </div>

        <div class="card">
            <h2>Your Profile</h2>
            <table>
                <tr>
                    <th>Field</th>
                    <th>Value</th>
                </tr>
                <tr>
                    <td>Username</td>
                    <td>
                        <?php echo htmlspecialchars($username); ?>
                    </td>
                </tr>
                <tr>
                    <td>User ID</td>
                    <td>
                        <?php echo $_SESSION['user_id']; ?>
                    </td>
                </tr>
                <tr>
                    <td>Session Started</td>
                    <td>
                        <?php echo date('Y-m-d H:i:s'); ?>
                    </td>
                </tr>
            </table>
        </div>

        <div class="card">
            <h2>About HybridDB</h2>
            <p><strong>Features:</strong></p>
            <ul style="margin: 15px 0 15px 20px; line-height: 1.8;">
                <li>✅ Complete C++ implementation (not PHP!)</li>
                <li>✅ Binary storage with 8KB pages</li>
                <li>✅ Socket-based client-server architecture</li>
                <li>✅ ACID transactions</li>
                <li>✅ Write-Ahead Logging (WAL)</li>
                <li>✅ Buffer pool caching</li>
                <li>✅ Multi-client support</li>
                <li>✅ Thin client libraries (PHP, Python, C++)</li>
            </ul>
        </div>
    </div>
</body>

</html>