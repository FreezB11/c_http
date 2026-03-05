#include <v1/framework.h>

void AuthHandler(Request req, Response res){

}

int main(){
    HTTP app;
    app.handle("GET","/", [](Request req, Response res){
        
    });

    app.handle("GET","/auth", *AuthHandler);
}