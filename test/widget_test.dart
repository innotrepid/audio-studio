import 'package:audio_studio/main.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('shows the Audio Studio foundation screen', (tester) async {
    await tester.pumpWidget(const AudioStudioApp());

    expect(find.text('Audio Studio'), findsOneWidget);
    expect(find.text('Your voice. Your studio.'), findsOneWidget);
    expect(find.text('Studio'), findsOneWidget);
    expect(find.text('Audio cleanup'), findsOneWidget);
    expect(find.text('Script and takes'), findsOneWidget);
    expect(find.text('Creator tools'), findsOneWidget);
    expect(
      find.textContaining('This is the initial app shell.'),
      findsOneWidget,
    );
  });
}
