# Buddy - demo script (one page)

## Before the demo (5 min)
1. Phone hotspot ON (2.4 GHz), mobile data ON. Robot on USB / power bank. Read the IP on the OLED at boot.
2. Phone Chrome: open `http://<robot-ip>/`. If the IP changed, update `chrome://flags/#unsafely-treat-insecure-origin-as-secure` for the mic.
3. Settings: API key saved, model `gemini-3.8-flash`, Voice = Robot speaker (retro), **Robot sound effects ON**.
4. Mind tab: **Reset learning** (fresh bars), learning mode stays **OFF** until the learning demo.
5. Battery cell IN only for the movement parts; nothing in front of the ultrasonic sensor; rear IR clear.

## Demo order (cognition first) - what to do / what to say
1. **Architecture (30 s).** "Perceive -> feel -> decide -> act -> adapt. All feelings, memory and learning run on the ESP32 itself; the phone is only ears, mouth and the link to the AI." Show the Mind tab (works offline).
2. **Perception -> emotions.** Slow hand to 50 cm -> *curious*; hold still at 20-25 cm -> *pet* -> *happy* (wiggle, chirp, green); fast hand -> *scared* (reverse if the rear is clear, red). Point at the bars and "because ..." line.
3. **Memory / bond.** Show pets/bond in the Mind tab, press **EN** -> after reboot bond and the memory are still there ("woke up", same bond).
4. **Learning.** Turn **Learning mode ON**. Leave Buddy alone ~20 s -> it tries a trick -> "Rate my move!" -> thumbs-up the same trick each time, thumbs-down the others -> its bar grows, thoughts say "Chose ... because it scored best". Say: "reinforcement learning: score moves 30 % towards each reward, explores 10-30 %". Turn learning **OFF** afterwards.
5. **User mood.** Chat "I have an exam tomorrow and I'm so stressed" -> calm reply, card *Stressed* with the quote. Then "That was fun, I'm feeling great now!" -> card *Happy*, playful reply.
6. **Focus presence.** Study tab -> **1 min demo**. Sit in front, then walk away -> "Hey, come back!" (sad, blue) -> come back -> "Welcome back!" -> score spoken + on the OLED. Next session nudges sooner if the score was < 70 % ("adapted").
7. **Then the extras:** chat/voice (mic button), Quiz (3 right in a row -> excited + bond +1), Explain, Drive (D-pad).

## Fallbacks
- AI busy/slow: the page tries other Gemini models by itself; if all fail, show the Mind tab + emotions (offline) and say "the AI is cloud-based".
- Robot voice unclear: Settings -> Voice = Phone speaker.
- Mic blocked on the phone: type instead (or fix the chrome://flags entry with the current IP).
- Too noisy: Settings -> Robot sound effects OFF.
- Robot unreachable: check the hotspot, reboot (EN), read the new IP on the OLED.
- Motors do nothing: battery cell in? Wheels lifted for a desk demo.
- Rear IR always "OBST": something behind the robot / turn its blue pot slightly anticlockwise.

## Known limitations (say them honestly)
One narrow ultrasonic beam (hand must be straight in front); on-board mic only senses low sounds (speech goes through the phone); free-tier AI can be slow; learning is a simple bandit, not deep learning.
