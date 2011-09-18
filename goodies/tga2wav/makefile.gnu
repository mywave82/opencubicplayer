CXXFLAGS+=	-Wall -O2

tga2wav:	tga2wav.o
	$(CXX) -o $@ $^ -lm

clean:
	rm -f tga2wav *.o *~ *.wav
