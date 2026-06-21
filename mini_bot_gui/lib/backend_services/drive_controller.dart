import 'dart:async';

import 'package:flutter/foundation.dart';

import '../models/drive_state.dart';
import 'tcp_client.dart';

/// Jog direction, as sent in `JOG:<D>,<S>`.
enum JogDirection { forward, backward }

/// Acceleration profile selector for `PROFILE:<P>`.
enum ProfileSelection { lin, trap, sin }

extension on ProfileSelection {
  String get wireValue {
    switch (this) {
      case ProfileSelection.lin:
        return 'LIN';
      case ProfileSelection.trap:
        return 'TRAP';
      case ProfileSelection.sin:
        return 'SIN';
    }
  }
}

extension on JogDirection {
  String get wireValue {
    switch (this) {
      case JogDirection.forward:
        return 'F';
      case JogDirection.backward:
        return 'B';
    }
  }
}

/// Owns the TCP connection to the linear drive and exposes the latest
/// [DriveState] plus command methods that send protocol lines.
///
/// All command strings and parsed status/event lines match
/// docs/PROTOCOL.md exactly.
class DriveController extends ChangeNotifier {
  DriveController() {
    _connectionSubscription =
        _client.connectionStateStream.listen(_onConnectionStateChanged);
    _lineSubscription = _client.lines.listen(_onLine);
  }

  final TcpClient _client = TcpClient();

  StreamSubscription<TcpConnectionState>? _connectionSubscription;
  StreamSubscription<String>? _lineSubscription;

  DriveState _state = DriveState();
  TcpConnectionState _connectionState = TcpConnectionState.disconnected;

  /// The latest known drive state.
  DriveState get state => _state;

  /// The current TCP connection state.
  TcpConnectionState get connectionState => _connectionState;

  /// Whether the controller is currently connected to the device.
  bool get isConnected => _connectionState == TcpConnectionState.connected;

  void _onConnectionStateChanged(TcpConnectionState newState) {
    // The device keeps its own state across disconnects; on reconnect we
    // simply re-sync from the next ST: line.
    _connectionState = newState;
    notifyListeners();
  }

  void _onLine(String line) {
    if (line.startsWith('ST:')) {
      _state = _state.applyStatusLine(line);
      notifyListeners();
    } else if (line.startsWith('EVT:')) {
      _state = _state.applyEventLine(line);
      notifyListeners();
    }
    // Unknown lines are ignored per protocol.
  }

  /// Connect to the device at [host]:[port]. Returns true on success.
  Future<bool> connect(String host, int port) async {
    final ok = await _client.connect(host, port);
    if (ok) {
      ping();
    }
    return ok;
  }

  /// Disconnect from the device.
  Future<void> disconnect() async {
    await _client.disconnect();
  }

  // ---------------------------------------------------------------------
  // Commands (see docs/PROTOCOL.md "Commands" table)
  // ---------------------------------------------------------------------

  /// `CAL` — start the calibration routine.
  void calibrate() => _client.send('CAL');

  /// `HOME` — start the homing routine.
  void home() => _client.send('HOME');

  /// `JOG:<D>,<S>` — jog continuously. Must be re-sent at least every
  /// 250 ms while the button is held.
  void jog(JogDirection direction, int speedPct) {
    final clamped = speedPct.clamp(0, 100);
    _client.send('JOG:${direction.wireValue},$clamped');
  }

  /// `JSTOP` — stop jogging immediately (graceful decel).
  void jogStop() => _client.send('JSTOP');

  /// `MOVE:<cm>,<S>` — move to absolute position `cm` at speed `S`.
  void moveTo(double cm, int speedPct) {
    final clamped = speedPct.clamp(0, 100);
    _client.send('MOVE:${cm.toString()},$clamped');
  }

  /// `STOP` — graceful stop of any active move/jog. Does not latch.
  void stop() => _client.send('STOP');

  /// `PROFILE:<P>` — set acceleration profile.
  void setProfile(ProfileSelection profile) =>
      _client.send('PROFILE:${profile.wireValue}');

  /// `ESTOP` — engage software e-stop. Latches.
  void estop() => _client.send('ESTOP');

  /// `ECLR` — clear the software e-stop.
  void clearEstop() => _client.send('ECLR');

  /// `PING` — connectivity check; device replies with an `ST:` line.
  void ping() => _client.send('PING');

  @override
  void dispose() {
    _connectionSubscription?.cancel();
    _lineSubscription?.cancel();
    _client.dispose();
    super.dispose();
  }
}
