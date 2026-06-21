import 'dart:async';
import 'dart:convert';
import 'dart:io';

/// Connection lifecycle states for [TcpClient].
enum TcpConnectionState { disconnected, connecting, connected }

/// A small bidirectional TCP client for the Linear Drive protocol.
///
/// Buffers incoming bytes, splits on '\n', strips a trailing '\r' and
/// emits complete lines on [lines]. Connection state is exposed via
/// [connectionState] and [connectionStateStream].
class TcpClient {
  Socket? _socket;
  StreamSubscription<List<int>>? _socketSubscription;

  final StreamController<String> _lineController =
      StreamController<String>.broadcast();
  final StreamController<TcpConnectionState> _stateController =
      StreamController<TcpConnectionState>.broadcast();

  TcpConnectionState _connectionState = TcpConnectionState.disconnected;

  String _buffer = '';

  /// Stream of complete inbound lines (without line terminators).
  Stream<String> get lines => _lineController.stream;

  /// Stream of connection-state changes.
  Stream<TcpConnectionState> get connectionStateStream =>
      _stateController.stream;

  /// The current connection state.
  TcpConnectionState get connectionState => _connectionState;

  void _setState(TcpConnectionState newState) {
    _connectionState = newState;
    if (!_stateController.isClosed) {
      _stateController.add(newState);
    }
  }

  /// Connect to [host]:[port]. Returns true on success, false otherwise.
  Future<bool> connect(String host, int port) async {
    if (_connectionState != TcpConnectionState.disconnected) {
      await disconnect();
    }

    _setState(TcpConnectionState.connecting);

    try {
      final socket = await Socket.connect(host, port,
          timeout: const Duration(seconds: 5));
      _socket = socket;
      _buffer = '';

      _socketSubscription = socket.listen(
        _onData,
        onError: (_) => _handleDisconnect(),
        onDone: _handleDisconnect,
        cancelOnError: true,
      );

      _setState(TcpConnectionState.connected);
      return true;
    } catch (_) {
      _socket = null;
      _setState(TcpConnectionState.disconnected);
      return false;
    }
  }

  void _onData(List<int> data) {
    _buffer += utf8.decode(data, allowMalformed: true);

    int newlineIndex;
    while ((newlineIndex = _buffer.indexOf('\n')) != -1) {
      var line = _buffer.substring(0, newlineIndex);
      _buffer = _buffer.substring(newlineIndex + 1);

      if (line.endsWith('\r')) {
        line = line.substring(0, line.length - 1);
      }

      if (line.isNotEmpty && !_lineController.isClosed) {
        _lineController.add(line);
      }
    }
  }

  void _handleDisconnect() {
    _socketSubscription?.cancel();
    _socketSubscription = null;
    _socket = null;
    if (_connectionState != TcpConnectionState.disconnected) {
      _setState(TcpConnectionState.disconnected);
    }
  }

  /// Send a command line. Appends '\n'. Does nothing if not connected.
  void send(String command) {
    final socket = _socket;
    if (socket == null || _connectionState != TcpConnectionState.connected) {
      return;
    }
    try {
      socket.write('$command\n');
    } catch (_) {
      _handleDisconnect();
    }
  }

  /// Disconnect from the device. Safe to call when not connected.
  Future<void> disconnect() async {
    await _socketSubscription?.cancel();
    _socketSubscription = null;
    try {
      await _socket?.close();
    } catch (_) {
      // ignore
    }
    _socket = null;
    _buffer = '';
    _setState(TcpConnectionState.disconnected);
  }

  /// Dispose of internal stream controllers. Call when the client is
  /// no longer needed.
  void dispose() {
    _socketSubscription?.cancel();
    _socket?.destroy();
    _lineController.close();
    _stateController.close();
  }
}
