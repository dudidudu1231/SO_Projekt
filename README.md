Opracować zestaw programów typu producent - konsument realizujących przy wykorzystaniu mechanizmu łączy nazwanych (kolejek FIFO) (między procesami P1-P2) oraz mechanizmu semaforów i pamięci dzielonej (między procesami P2-P3) następujący schemat komunikacji międzyprocesowej:

Proces 1:             czyta dane (pojedyncze wiersze) ze standardowego strumienia wejściowego lub pliku i przekazuje je w niezmienionej formie do procesu 2.

Proces 2:             pobiera dane przesłane przez proces 1. Oblicza ilość znaków w każdej linii i wyznaczoną liczbę przekazuje do procesu 3.

Proces 3:             pobiera dane wyprodukowane przez proces 2 i umieszcza je w standardowym strumieniu wyjściowym. Każda odebrana jednostka danych powinna zostać wyprowadzona w osobnym wierszu.

Należy zaproponować i zaimplementować mechanizm informowania się procesów o swoim stanie. Należy wykorzystać do tego dostępny mechanizm sygnałów i kolejek komunikatów. Scenariusz powiadamiania się procesów o swoim stanie wygląda następująco: do procesu 3 wysyłane są sygnały. Proces 3 przesyła otrzymany sygnał do procesu macierzystego. Proces macierzysty zapisuje wartość sygnału do kolejek komunikatów oraz wysyła powiadomienie do procesu 1 o odczytaniu zawartości kolejki komunikatów. Proces 1 po odczytaniu sygnału wysyła powiadomienie do procesu 2 o odczytanie kolejki komunikatów. Proces 2 powiadamia proces 3 o konieczności odczytu kolejki komunikatów. Wszystkie trzy procesy powinny być powoływane automatycznie z jednego procesu inicjującego.
