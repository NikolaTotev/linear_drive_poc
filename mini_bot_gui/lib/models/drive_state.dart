/// Operating state of the linear drive, as reported in the `state` field
/// of an `ST:` line.
enum DriveOperatingState {
  idle,
  homing,
  calibrating,
  jogging,
  moving,
  estop,
  error,
  unknown,
}

/// Acceleration profile, as reported in the `profile` field of an `ST:`
/// line / set via `PROFILE:<P>`.
enum DriveProfile { lin, trap, sin, unknown }

DriveOperatingState _parseOperatingState(String raw) {
  switch (raw) {
    case 'IDLE':
      return DriveOperatingState.idle;
    case 'HOMING':
      return DriveOperatingState.homing;
    case 'CALIBRATING':
      return DriveOperatingState.calibrating;
    case 'JOGGING':
      return DriveOperatingState.jogging;
    case 'MOVING':
      return DriveOperatingState.moving;
    case 'ESTOP':
      return DriveOperatingState.estop;
    case 'ERROR':
      return DriveOperatingState.error;
    default:
      return DriveOperatingState.unknown;
  }
}

DriveProfile _parseProfile(String raw) {
  switch (raw) {
    case 'LIN':
      return DriveProfile.lin;
    case 'TRAP':
      return DriveProfile.trap;
    case 'SIN':
      return DriveProfile.sin;
    default:
      return DriveProfile.unknown;
  }
}

/// Which endstop was hit, from `EVT:LIMIT,<W>`.
enum LimitSide { min, max, unknown }

LimitSide _parseLimitSide(String raw) {
  switch (raw) {
    case 'MIN':
      return LimitSide.min;
    case 'MAX':
      return LimitSide.max;
    default:
      return LimitSide.unknown;
  }
}

/// Reactive state of the linear drive, parsed from `ST:` and `EVT:` lines
/// pushed by the device. See docs/PROTOCOL.md for the wire format.
class DriveState {
  /// Current operating state.
  DriveOperatingState state;

  /// Current carriage position in cm from MIN. `-1` if unknown
  /// (not homed).
  double posCm;

  /// Whether the drive has been homed.
  bool homed;

  /// Whether software or physical e-stop is latched.
  bool estop;

  /// Currently-selected acceleration profile.
  DriveProfile profile;

  /// Full-travel encoder count from calibration. `0` if uncalibrated.
  int rangePulses;

  /// Last error code reported by the device, or `NONE`.
  String err;

  /// Set when an `EVT:LIMIT` occurs: an unexpected endstop hit that
  /// requires the user to re-home before further motion. Cleared once
  /// [homed] becomes true again (i.e. after `EVT:HOMED`).
  bool needsRehome;

  /// Which side the unexpected limit was hit on, if [needsRehome] is set.
  LimitSide limitSide;

  DriveState({
    this.state = DriveOperatingState.unknown,
    this.posCm = -1,
    this.homed = false,
    this.estop = false,
    this.profile = DriveProfile.unknown,
    this.rangePulses = 0,
    this.err = 'NONE',
    this.needsRehome = false,
    this.limitSide = LimitSide.unknown,
  });

  /// Whether the position is currently known/valid.
  bool get hasValidPosition => posCm >= 0 && homed;

  /// Whether the device has been calibrated at least once.
  bool get isCalibrated => rangePulses > 0;

  /// Parse an `ST:<state>,<pos_cm>,<homed>,<estop>,<profile>,<range_pulses>,<err>`
  /// line and return a new [DriveState] reflecting the update.
  ///
  /// Preserves [needsRehome] / [limitSide] unless the new status implies
  /// the rehome requirement should be cleared (i.e. the device reports
  /// `homed=1`).
  DriveState applyStatusLine(String line) {
    final body = line.substring('ST:'.length);
    final fields = body.split(',');
    if (fields.length != 7) {
      // Malformed / unexpected line: ignore per protocol ("unknown lines
      // must be ignored").
      return this;
    }

    final newState = _parseOperatingState(fields[0]);
    final newPos = double.tryParse(fields[1]) ?? posCm;
    final newHomed = fields[2] == '1';
    final newEstop = fields[3] == '1';
    final newProfile = _parseProfile(fields[4]);
    final newRange = int.tryParse(fields[5]) ?? rangePulses;
    final newErr = fields[6];

    return DriveState(
      state: newState,
      posCm: newPos,
      homed: newHomed,
      estop: newEstop,
      profile: newProfile,
      rangePulses: newRange,
      err: newErr,
      needsRehome: newHomed ? false : needsRehome,
      limitSide: newHomed ? LimitSide.unknown : limitSide,
    );
  }

  /// Parse an `EVT:<NAME>[,<args>]` line and return a new [DriveState]
  /// reflecting the update. Unknown event names are ignored.
  DriveState applyEventLine(String line) {
    final body = line.substring('EVT:'.length);
    final parts = body.split(',');
    final name = parts[0];

    switch (name) {
      case 'CAL_DONE':
        final pulses =
            parts.length > 1 ? int.tryParse(parts[1]) ?? rangePulses : rangePulses;
        return DriveState(
          state: state,
          posCm: posCm,
          homed: homed,
          estop: estop,
          profile: profile,
          rangePulses: pulses,
          err: err,
          needsRehome: needsRehome,
          limitSide: limitSide,
        );

      case 'HOMED':
        return DriveState(
          state: state,
          posCm: posCm,
          homed: true,
          estop: estop,
          profile: profile,
          rangePulses: rangePulses,
          err: err,
          needsRehome: false,
          limitSide: LimitSide.unknown,
        );

      case 'LIMIT':
        final side =
            parts.length > 1 ? _parseLimitSide(parts[1]) : LimitSide.unknown;
        return DriveState(
          state: state,
          posCm: posCm,
          homed: homed,
          estop: estop,
          profile: profile,
          rangePulses: rangePulses,
          err: err,
          needsRehome: true,
          limitSide: side,
        );

      case 'ESTOP':
        return DriveState(
          state: state,
          posCm: posCm,
          homed: homed,
          estop: true,
          profile: profile,
          rangePulses: rangePulses,
          err: err,
          needsRehome: needsRehome,
          limitSide: limitSide,
        );

      case 'ECLR':
        return DriveState(
          state: state,
          posCm: posCm,
          homed: homed,
          estop: false,
          profile: profile,
          rangePulses: rangePulses,
          err: err,
          needsRehome: needsRehome,
          limitSide: limitSide,
        );

      case 'ERR':
        final code = parts.length > 1 ? parts[1] : err;
        return DriveState(
          state: state,
          posCm: posCm,
          homed: homed,
          estop: estop,
          profile: profile,
          rangePulses: rangePulses,
          err: code,
          needsRehome: needsRehome,
          limitSide: limitSide,
        );

      default:
        // Unknown event: ignore.
        return this;
    }
  }

  /// Copy this state, optionally overriding fields.
  DriveState copyWith({
    DriveOperatingState? state,
    double? posCm,
    bool? homed,
    bool? estop,
    DriveProfile? profile,
    int? rangePulses,
    String? err,
    bool? needsRehome,
    LimitSide? limitSide,
  }) {
    return DriveState(
      state: state ?? this.state,
      posCm: posCm ?? this.posCm,
      homed: homed ?? this.homed,
      estop: estop ?? this.estop,
      profile: profile ?? this.profile,
      rangePulses: rangePulses ?? this.rangePulses,
      err: err ?? this.err,
      needsRehome: needsRehome ?? this.needsRehome,
      limitSide: limitSide ?? this.limitSide,
    );
  }
}
