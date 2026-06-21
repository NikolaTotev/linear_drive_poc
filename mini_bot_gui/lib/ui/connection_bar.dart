import 'package:flutter/material.dart';
import 'package:ocadi_bot_gui/app_constants/colors.dart' as mini_colors;
import 'package:ocadi_bot_gui/backend_services/drive_controller.dart';
import 'package:ocadi_bot_gui/backend_services/tcp_client.dart';

import 'section_heading.dart';

/// Connection bar: editable host + port, Connect/Disconnect button and a
/// clear connection-status indicator.
class ConnectionBar extends StatefulWidget {
  const ConnectionBar({super.key, required this.controller});

  final DriveController controller;

  @override
  State<ConnectionBar> createState() => _ConnectionBarState();
}

class _ConnectionBarState extends State<ConnectionBar> {
  final TextEditingController _hostController =
      TextEditingController(text: '192.168.4.1');
  final TextEditingController _portController =
      TextEditingController(text: '5000');

  bool _connecting = false;

  @override
  void dispose() {
    _hostController.dispose();
    _portController.dispose();
    super.dispose();
  }

  Future<void> _onConnectPressed() async {
    final state = widget.controller.connectionState;
    if (state == TcpConnectionState.connected ||
        state == TcpConnectionState.connecting) {
      await widget.controller.disconnect();
      return;
    }

    final host = _hostController.text.trim();
    final port = int.tryParse(_portController.text.trim());
    if (host.isEmpty || port == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Enter a valid host and port')),
      );
      return;
    }

    setState(() => _connecting = true);
    final ok = await widget.controller.connect(host, port);
    setState(() => _connecting = false);

    if (!ok && mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Could not connect to $host:$port')),
      );
    }
  }

  Widget _statusIndicator(TcpConnectionState state) {
    Color color;
    String label;
    switch (state) {
      case TcpConnectionState.connected:
        color = Colors.green;
        label = 'Connected';
        break;
      case TcpConnectionState.connecting:
        color = Colors.orange;
        label = 'Connecting...';
        break;
      case TcpConnectionState.disconnected:
        color = Colors.red;
        label = 'Disconnected';
        break;
    }

    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(
          width: 14,
          height: 14,
          decoration: BoxDecoration(
            color: color,
            shape: BoxShape.circle,
          ),
        ),
        const SizedBox(width: 8),
        Text(
          label,
          style: const TextStyle(
            fontWeight: FontWeight.w600,
            color: mini_colors.notQuiteBlack,
          ),
        ),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    final connState = widget.controller.connectionState;
    final isBusy = _connecting || connState == TcpConnectionState.connecting;
    final isConnected = connState == TcpConnectionState.connected;

    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16.0),
      child: Column(
        children: [
          const SectionHeading(title: 'Connection'),
          Wrap(
            alignment: WrapAlignment.center,
            crossAxisAlignment: WrapCrossAlignment.center,
            spacing: 12,
            runSpacing: 8,
            children: [
              SizedBox(
                width: 160,
                child: TextField(
                  controller: _hostController,
                  enabled: !isConnected,
                  decoration: const InputDecoration(
                    labelText: 'Host',
                    border: OutlineInputBorder(),
                    isDense: true,
                  ),
                ),
              ),
              SizedBox(
                width: 100,
                child: TextField(
                  controller: _portController,
                  enabled: !isConnected,
                  keyboardType: TextInputType.number,
                  decoration: const InputDecoration(
                    labelText: 'Port',
                    border: OutlineInputBorder(),
                    isDense: true,
                  ),
                ),
              ),
              ElevatedButton(
                onPressed: isBusy ? null : _onConnectPressed,
                style: ElevatedButton.styleFrom(
                  backgroundColor: mini_colors.darkRoyalPurple,
                  foregroundColor: mini_colors.offWhite,
                ),
                child: Text(isConnected ? 'Disconnect' : 'Connect'),
              ),
              _statusIndicator(connState),
            ],
          ),
          const SizedBox(height: 8),
        ],
      ),
    );
  }
}
