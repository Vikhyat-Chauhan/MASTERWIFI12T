/*  
 *	Name: HomeHub First Example
 *  Date Created: 9/11/2019
 *  Description: Example code using the HomeHub library.
 * 
 *		See: https://www.thenextmove.in
 */
#include "HomeHub.h"
#include <Ticker.h>

//  Create an instance of the myFirstLibrary class called 'mySetup' 
HomeHub homehub;

void setup(){
} 

void loop(){
  // Run the on function
  homehub.asynctasks();
}
